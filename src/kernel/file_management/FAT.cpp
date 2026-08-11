#include "FAT.h"
#include <kstddef.h>
#include "ATA.h"
#include <kstring.h>
#include "superblock.h"
#include "dentry.h"
#include "../core/fb.h"
#include "../core/memory/memory.h"
#include "../utils/comparison.h"
#include "../utils/TmpString.h"
#include <dirent.h>

#include "ATA_Drive.h"
#include "BlockDevice.h"
#include "USB_Drive.h"
#include "USB/USB.h"
#include "USB/xHCI.h"

#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

list<FAT_drive*>* FAT_drive::drives = nullptr;

#define ERR_RET_FALSE(err_msg) \
    {                          \
        printf_error("%s: %s", __func__, err_msg); \
        return false;          \
    }

#define ERR_RET_NULL(err_msg) \
    {                          \
        printf_error("%s: %s", __func__, err_msg); \
        return nullptr;        \
    }

TmpString LongDirEntry::utf16_to_utf8_cautionless_cast(const char* str, const uint length)
{
    const uint half_length = length / 2;
    const uint n = half_length + 1;
    TmpString tmp_string{n};
    char* res = *tmp_string;;
    for (uint i = 0; i < half_length; ++i)
    {
        res[i] = str[i * 2];
        if (str[i * 2] == '\0' && str[i * 2 + 1] == '\0')
        {
            res[i] = '\0';
            return tmp_string;
        }
    }
    res[half_length] = '\0';

    return tmp_string;
}

TmpString LongDirEntry::get_uglily_converted_utf8_name() const
{
    const auto n1 = utf16_to_utf8_cautionless_cast(name1, 10);

    TmpString full_name(10 + 12 + 4 + 1);
    memset(*full_name, 0, 10 + 12 + 4 + 1);
    uint p = 0, gp = 0;
    while (p < 10 && (*n1)[p])
        (*full_name)[gp++] = (*n1)[p++];

    if (strlen(*n1) >= 5)
    {
        const auto n2 = utf16_to_utf8_cautionless_cast(name2, 12);
        p = 0;
        while (p < 12 && (*n2)[p])
            (*full_name)[gp++] = (*n2)[p++];

        if (*n2 && strlen(*n2) >= 6)
        {
            const auto n3 = utf16_to_utf8_cautionless_cast(name3, 4);
            p = 0;
            while (p < 4 && (*n3)[p])
                (*full_name)[gp++] = (*n3)[p++];
        }
    }

    return full_name;
}

bool LongDirEntry::is_EOF() const
{
    return order & LAST_LONG_ENTRY;
}

Result<FAT_drive*> FAT_drive::from_block_device(BlockDevice* dev)
{
    unsigned char buf[FAT_SECTOR_SIZE];
    if (dev->get_block_size() != FAT_SECTOR_SIZE)
        return MAKE_ERR("FAT driver only supports device with a block size of %u bytes for now, got %u", ATA_SECTOR_SIZE, dev->get_block_size());

    TRY(dev->read_blocks(0, 1, buf));
    auto* fat_boot = new fat_BS_t;
    memcpy(fat_boot, buf, sizeof(*fat_boot));

    auto* extBS_32 = (fat_extBS_32*)&fat_boot->extended_section;
    uint total_sectors = (fat_boot->total_sectors_16 == 0) ? fat_boot->total_sectors_32 : fat_boot->total_sectors_16;
    uint root_dir_sectors =
        ((fat_boot->root_entry_count * 32) + (fat_boot->bytes_per_sector - 1)) / fat_boot->bytes_per_sector;
    uint fat_size = (fat_boot->table_size_16 == 0) ? extBS_32->table_size_32 : fat_boot->table_size_16;
    uint data_sectors =
        total_sectors - (fat_boot->reserved_sector_count + (fat_boot->table_count * fat_size) + root_dir_sectors);
    uint total_clusters = data_sectors / fat_boot->sectors_per_cluster;
    FAT_type fat_type;
    if (fat_boot->bytes_per_sector == 0)
        fat_type = ExFAT;
    else if (total_clusters < 4085)
        fat_type = FAT12;
    else if (total_clusters < 65525)
        fat_type = FAT16;
    else
        fat_type = FAT32;

    if (fat_type != FAT32)
        return make_ok((FAT_drive*)nullptr);

    return make_ok(new FAT_drive(dev, fat_boot));
}

const char** FAT_drive::split_at_slashes(const char* str, uint* num_tokens)
{
    uint num_slashes = 0;
    uint l = strlen(str);
    for (uint i = 0; i < l; ++i)
        if (str[i] == '/')
            num_slashes++;

    if (!num_slashes)
        return nullptr;

    char** res = new char*[num_slashes + 1];

    // Get tokens
    uint tok_id = 0; // Current token ID
    uint beg = 0, end; // Current token start and end index
    for (uint i = 0; i < l; ++i)
    {
        if (str[i] == '/')
        {
            end = i;
            char* token = new char[end - beg + 1];
            token[end - beg] = '\0';
            memcpy(token, str + beg, sizeof(char) * (end - beg));
            beg = i + 1;
            res[tok_id++] = token;
        }
        else
            end++;
    }
    // Handle last token (the one after the last slash)
    res[tok_id] = new char[l - beg + 1];
    memcpy(res[tok_id], str + beg, sizeof(char) * (l - beg));
    res[tok_id][l - beg] = '\0';

    *num_tokens = num_slashes + 1;
    return (const char**)res;
}

Result<uint*> FAT_drive::get_free_clusters(size_t n) const
{
    if (n == 0)
        return make_ok((uint*)calloc(1, sizeof(uint)));

    auto free_cluster_list = new uint[n];
    size_t free_clusters_found = 0;
    size_t num_fat_entries_per_sector = FAT_SECTOR_SIZE / sizeof(uint32_t);
    for (uint sector = first_fat_sector; sector < total_sectors; ++sector)
    {
        // Get FAT sector
        uint32_t fat_buf[num_fat_entries_per_sector];
        TRY(dev->read_blocks(sector, 1, fat_buf));
        // Read FAT entries
        for (uint j = 0; j < num_fat_entries_per_sector; j++)
            if (!fat_buf[j]) // Found a free entry
            {
                if (sector == first_fat_sector && j < 2)
                    continue; // Reserved entries
                uint cluster_number = (sector - first_fat_sector) * num_fat_entries_per_sector + j;
                free_cluster_list[free_clusters_found++] = cluster_number;

                // Enough clusters found
                if (free_clusters_found == n)
                    return make_ok(free_cluster_list);
            }
    }

    delete[] free_cluster_list;
    return MAKE_ERR("Not enough free clusters found, requested %zu, found %zu", n, free_clusters_found);
}

FAT_drive::FAT_drive(BlockDevice* dev, fat_BS_t* bs) : FS(FAT_SECTOR_SIZE, dev),
                                                       bs(*bs),
                                                       extBS_32(*(fat_extBS_32*)this->bs.extended_section),
                                                       total_sectors((bs->total_sectors_16 == 0)
                                                                         ? bs->total_sectors_32
                                                                         : bs->total_sectors_16),
                                                       fat_size((bs->table_size_16 == 0)
                                                                    ? extBS_32.table_size_32
                                                                    : bs->table_size_16),
                                                       root_dir_sectors(((bs->root_entry_count * 32) +
                                                               (bs->bytes_per_sector - 1)) /
                                                           bs->bytes_per_sector),
                                                       first_data_sector(
                                                           bs->reserved_sector_count +
                                                           (bs->table_count * fat_size) +
                                                           root_dir_sectors),
                                                       first_fat_sector(bs->reserved_sector_count),
                                                       data_sectors(
                                                           total_sectors -
                                                           (bs->reserved_sector_count +
                                                               (bs->table_count * fat_size) +
                                                               root_dir_sectors)),
                                                       total_clusters(data_sectors / bs->sectors_per_cluster),
                                                       buf(new char[FAT_SECTOR_SIZE]),
                                                       entries((DirEntry*)buf),
                                                       FAT(new unsigned char[FAT_SECTOR_SIZE])
{
    if (bs->sectors_per_cluster != 1)
        irrecoverable_error("FAT driver only supports 1 sector per cluster, current value: %d.\n"
                     ". Drive accesses could misbehave.", bs->sectors_per_cluster);

    if (bs->bytes_per_sector != FAT_SECTOR_SIZE)
        irrecoverable_error("FAT driver only supports %d bytes per sector, current value: %d.\n"
                     ". Drive accesses could misbehave.", FAT_SECTOR_SIZE, bs->bytes_per_sector);
}

FAT_drive::~FAT_drive()
{
    delete[] FAT;
    delete[] buf;

    fs_list->remove(this);
}

Status FAT_drive::change_active_cluster(uint new_active_cluster, ctx& ctx, void* buffer) const
{
    if (!buffer)
    {
        ctx.buffer_updated = ctx.active_cluster != new_active_cluster;
        ctx.active_cluster = new_active_cluster;
    }
    // If active cluster changed or was updated on disk, read new cluster data
    else if (ctx.active_cluster != new_active_cluster || ctx.buffer_updated)
    {
        ctx.active_cluster = new_active_cluster;
        ctx.active_sector = FIRST_SECTOR_OF_CLUSTER(ctx.active_cluster, bs.sectors_per_cluster, first_data_sector);

        // Read data from drive
        TRY(dev->read_blocks(ctx.active_sector, 1, buffer));

        ctx.buffer_updated = false;
    }

    // Read FAT sector
    uint FAT_offset = ctx.active_cluster * sizeof(uint32_t);
    uint new_FAT_sector = first_fat_sector + (FAT_offset / FAT_SECTOR_SIZE);
    ctx.FAT_entry_offset = FAT_offset % FAT_SECTOR_SIZE;
    if (new_FAT_sector != ctx.FAT_sector)
    {
        ctx.FAT_sector = new_FAT_sector;
        TRY(dev->read_blocks(ctx.FAT_sector, 1, FAT));
    }

    ctx.table_value = *(uint*)&FAT[ctx.FAT_entry_offset];
    /*if (fat32) */
    ctx.table_value &= 0x0FFFFFFF;
    ctx.dir_entry_id = 0;

    return Status::success();
}

void FAT_drive::init()
{
    ATA::init();
    xHCI::get_instance()->start();
    USB::get_instance()->enumerate_devices();

    drives = new list<FAT_drive*>;

    // Add ATA devices
    for (uint i = 0; i < 4; ++i)
    {
        if (const auto drive = ATA::drive_present(i) && IDE::devices[i].Type == IDE_ATA
                                   ? from_block_device(
                                       new ATA_Drive(
                                           FAT_SECTOR_SIZE, MKDEV(DEV_ATA_PRIMARY_MASTER_MAJOR, i),
                                           MKDEV(DEV_ATA_PRIMARY_MASTER_MAJOR, i))
                                   ).expect()
                                   : nullptr)
            fs_list->add(drive);
    }

    // Add USB devices
    for (auto& usb_device : USB::get_instance()->get_mass_storage_devices())
    {
        if (const auto drive = from_block_device(
                                    new USB_Drive(
                                        FAT_SECTOR_SIZE,
                                        MKDEV(DEV_USB_MASS_STORAGE_MAJOR, usb_device.interface_number),
                                        &usb_device)
                                ).expect())
            fs_list->add(drive);
    }
}

void FAT_drive::shutdown()
{
    for (const auto drive : *drives)
        delete drive;
    delete drives;
}

Status FAT_drive::load_file_to_buf(void* buf, const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset,
                                   uint length, uint& loaded_bytes)
{
    ctx ctx{};
    uint file_entry_id;
    if ((file_entry_id = TRY(get_child_dir_entry_id(parent_dentry, file_name, ctx))) == ENTRY_NOT_FOUND)
        return Status::failure("File not found");
    DirEntry* file_entry = &entries[file_entry_id];
    uint next_cluster = file_entry->first_cluster_addr();
    if (offset + length > file_entry->file_size)
        return Status::failure("Offset + length exceeds file size");

    // Skip clusters until we reach the offset
    uint n_offset_clusters = offset / FAT_SECTOR_SIZE;
    uint cluster_offset = offset - n_offset_clusters * FAT_SECTOR_SIZE;
    uint co;
    for (co = 0; co < n_offset_clusters && next_cluster < CLUSTER_MIN_EOC; co++)
    {
        TRY(change_active_cluster(next_cluster, ctx, nullptr));
        next_cluster = ctx.table_value;
    }
    if (co < n_offset_clusters)
        return Status::failure("Offset exceeds file size");

    // If offset is 0 and length is greater than a sector, we can read the entire sector directly
    if (cluster_offset == 0 && length >= FAT_SECTOR_SIZE)
    {
        TRY(change_active_cluster(next_cluster, ctx, buf));
        loaded_bytes = FAT_SECTOR_SIZE;
    }
    else // If offset is not 0, we need to load the sector and copy the meaningful data to buf
    {
        TRY(change_active_cluster(next_cluster, ctx, this->buf));
        auto n = min(length, FAT_SECTOR_SIZE - cluster_offset);
        memcpy(buf, this->buf + cluster_offset, n);
        loaded_bytes = n;
    }
    next_cluster = ctx.table_value;

    //loaded_bytes = 0;
    auto b  = (char*)buf;
    while (loaded_bytes < length && next_cluster < CLUSTER_MIN_EOC)
    {
        // Load data into buffer. If there is more than ATA_SECTOR_SIZE data left to load, copy directly to b, otherwise
        // copy an entire sector to buf and copy the meaningful data to b
        uint rem = length - loaded_bytes;
        void* load_buf = rem < FAT_SECTOR_SIZE ? this->buf : b + loaded_bytes;
        TRY(change_active_cluster(next_cluster, ctx, load_buf));
        if (rem < FAT_SECTOR_SIZE) // Entire sector has been loaded to buf, copy meaningful data to b
            memcpy(b + loaded_bytes, this->buf, rem);
        loaded_bytes += rem > FAT_SECTOR_SIZE ? FAT_SECTOR_SIZE : rem;
        next_cluster = ctx.table_value;
    }

    return Status::success();
}

Result<uint> FAT_drive::get_child_dir_entry_id(const SharedPointer<Dentry>& parent_dentry, const char* name, ctx& ctx)
{
    // Make sure path makes sense
    if (!name || name[0] == '/')
        return make_ok(ENTRY_NOT_FOUND);

    uint parent_sector = parent_dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    uint curr_cluster = parent_cluster;

    // Skip used dir entries, aka files/folders inside wd
    do
    {
        // ~= cd wd
        TRY(change_active_cluster(curr_cluster, ctx, this->buf));

        bool found_in_lfn = false;
        TmpString whole_name(1);
        while (ctx.dir_entry_id * sizeof(DirEntry) < FAT_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free())
        {
            if (found_in_lfn) // File name matched in previous entry which is a fln entry referring to the current entry
                return make_ok(ctx.dir_entry_id);
            const bool lfn = entries[ctx.dir_entry_id].is_LFN();

            TmpString entry_name = lfn
                                   ? ((LongDirEntry*)&entries[ctx.dir_entry_id])->get_uglily_converted_utf8_name()
                                   : entries[ctx.dir_entry_id].get_name();
            if (lfn)
                whole_name = entry_name.concat(whole_name);
            else
                whole_name = entry_name;

            const bool match = !strcmp(*whole_name, name);

            if (!lfn)
                whole_name = TmpString(1); // Erase whole_name
            if (match)
            {
                found_in_lfn = lfn;
                if (!lfn)
                    return make_ok( ctx.dir_entry_id);
            }

            ctx.dir_entry_id++;
        }
        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    return make_ok(ENTRY_NOT_FOUND);
}

SharedPointer<Dentry> FAT_drive::get_child_dentry(SharedPointer<Dentry>& parent_dentry, const char* name)
{
    uint entry_id;
    if (ctx ctx{}; (entry_id = get_child_dir_entry_id(parent_dentry, name, ctx).expect()) == ENTRY_NOT_FOUND)
        return nullptr;
    return dir_entry_to_dentry(entries[entry_id], parent_dentry, name).expect();
}

Result<SharedPointer<Dentry>> FAT_drive::dir_entry_to_dentry(const DirEntry& dir_entry,
                                                             const SharedPointer<Dentry>& parent_dentry,
                                                             const char* name)
{
    Inode::Type inode_type = dir_entry.attrs & DIRECTORY ? Inode::Dir : Inode::File;

    // Count blocks
    blkcnt_t blocks;
    if (inode_type == Inode::File)
        blocks = (blkcnt_t)(dir_entry.file_size + 512 - 1) / 512; // 512 is the man page value, not ATA_SECTOR_SIZE
    else
    {
        blocks = 0;
        uint sector = dir_entry.first_cluster_addr();
        uint cluster = sector * bs.sectors_per_cluster;
        uint curr_cluster = cluster;
        ctx ctx{};

        // Count blocks until we reach the end of the cluster chain
        do
        {
            TRY(change_active_cluster(curr_cluster, ctx, nullptr));
            curr_cluster = ctx.table_value;
            blocks++;
        } while (curr_cluster < CLUSTER_MIN_EOC);
    }
    auto inode = new Inode(superblock, dir_entry.file_size, dir_entry.first_cluster_addr(), inode_type,
                           dir_entry.first_cluster_addr(), 1, 0, 0, 0, blocks, 0, 0, 0);

    return make_ok<SharedPointer<Dentry>>(new Dentry(inode, parent_dentry, name));
}

Status FAT_drive::write_fat(const ctx& ctx) const
{
    TRY(dev->write_blocks(ctx.FAT_sector, 1, FAT)); // Write new FAT
    return Status::success();
}

Status FAT_drive::write_data_sectors(uint numsects, uint lba, const void* buffer, ctx& ctx) const
{
    TRY(dev->write_blocks(lba, numsects, buffer));

    // If buffer is this->buf, then we already have the new data in memory. Otherwise, on a call to
    // change_active_cluster with new_cluster being equal to ctx.current_cluster, we will read the new data from disk,
    // which we indicate here
    ctx.buffer_updated = buffer != this->buf;

    return Status::success();
}

Result<SharedPointer<Dentry>> FAT_drive::touch(SharedPointer<Dentry>& parent_dentry, const char* entry_name)
{
    // Make sure path makes sense
    if (!entry_name || entry_name[0] == '/')
        return make_ok<SharedPointer<Dentry>>(nullptr);

    uint parent_sector = parent_dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    uint curr_cluster = parent_cluster;
    ctx ctx{};

    do
    {
        // ~= cd wd
        TRY(change_active_cluster(curr_cluster, ctx, this->buf));

        // Skip used dir entries, aka files/folders inside wd
        while (ctx.dir_entry_id * sizeof(DirEntry) < FAT_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free() &&
            strcmp(*entries[ctx.dir_entry_id].get_name(), entry_name) != 0)
            ctx.dir_entry_id++;

        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    // No free entry in wd cluster
    if (ctx.dir_entry_id * sizeof(DirEntry) == bs.bytes_per_sector * bs.sectors_per_cluster)
        MAKE_ERR("Working directory cluster is full, cluster chain extension implementation is needed");

    // Write file entry
    DirEntry new_entry(entry_name, 0, 0, 0);
    memcpy(&entries[ctx.dir_entry_id], &new_entry, sizeof(DirEntry));
    TRY(write_data_sectors(1, ctx.active_sector, buf, ctx));

    return dir_entry_to_dentry(new_entry, parent_dentry, entry_name);
}

Result<SharedPointer<Dentry>> FAT_drive::mkdir(SharedPointer<Dentry>& parent_dentry, const char* entry_name)
{
    uint l = strlen(entry_name);
    if (l >= 12 - 3)
        MAKE_ERR("Dir name requires LFN support");

    uint parent_sector = parent_dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    uint curr_cluster = parent_cluster;
    ctx ctx{};

    do
    {
        // ~= cd wd
        TRY(change_active_cluster(curr_cluster, ctx, this->buf));

        // Skip used dir entries, aka files/folders inside wd
        while (ctx.dir_entry_id * sizeof(DirEntry) < FAT_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free())
            ctx.dir_entry_id++;

        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    // No free entry in wd cluster
    if (ctx.dir_entry_id * sizeof(DirEntry) == bs.bytes_per_sector * bs.sectors_per_cluster)
        MAKE_ERR("Working directory cluster is full, cluster chain extension implementation is needed");

    auto free_clusters_list = TRY(get_free_clusters(1));
    if (free_clusters_list == nullptr)
        MAKE_ERR("No free cluster found");
    uint dir_content_cluster = free_clusters_list[0];
    delete[] free_clusters_list;

    // Write new entry
    DirEntry new_entry(entry_name, DIRECTORY, dir_content_cluster, 0);
    memcpy(&entries[ctx.dir_entry_id], &new_entry, sizeof(DirEntry));
    TRY(write_data_sectors(1, ctx.active_sector, buf, ctx));

    // ~= cd new directory
    TRY(change_active_cluster(dir_content_cluster, ctx, this->buf));

    uint dir_content_sector = ctx.active_sector;

    // Indicate that dir content cluster is the end of the cluster chain it belongs to
    *(uint*)&FAT[ctx.FAT_entry_offset] = CLUSTER_EOC;
    TRY(write_fat(ctx));

    // Note: I guess this is unnecessary since dir content cluster is supposed to be empty
    TRY(dev->read_blocks(dir_content_sector, 1, buf));

    // Create dot and dot dot entries
    DirEntry dot_entry(".", DIRECTORY, dir_content_cluster, 0);
    DirEntry dot_dot_entry("..", DIRECTORY, parent_cluster, 0);
    memcpy(&entries[0], &dot_entry, sizeof(DirEntry));
    memcpy(&entries[1], &dot_dot_entry, sizeof(DirEntry));

    // Write them to disk
    TRY(write_data_sectors(1, dir_content_sector, buf, ctx));

    return dir_entry_to_dentry(new_entry, parent_dentry, entry_name);
}

Status FAT_drive::ls(const SharedPointer<Dentry>& dentry, ls_printer printer)
{
    uint parent_sector = dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    ctx ctx{};

    uint curr_cluster = parent_cluster;
    do
    {
        // ~= cd wd
        TRY(change_active_cluster(curr_cluster, ctx, this->buf));

        TmpString prev_lfn(1);
        auto prev_is_lfn = [&prev_lfn]() {return **prev_lfn != '\0';};
        while (ctx.dir_entry_id * sizeof(DirEntry) < FAT_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free())
        {
            if (const auto entry = entries + ctx.dir_entry_id; entry->is_LFN())
                prev_lfn = ((LongDirEntry*)entry)->get_uglily_converted_utf8_name().concat(prev_lfn);
            else
            {
                TmpString entry_name = prev_is_lfn() ? prev_lfn : entry->get_name();
                SharedPointer<Dentry> null_parent = {nullptr};
                SharedPointer<Dentry> dir_dentry = TRY(dir_entry_to_dentry(*entry, null_parent, *entry_name));
                printer(*dir_dentry);
                prev_lfn = TmpString(1);
            }

            ctx.dir_entry_id++;
        }
        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    return Status::success();
}

bool FAT_drive::drive_present(uint drive_id)
{
    return drives->get(drive_id);
}

SharedPointer<Inode> FAT_drive::get_root_node()
{
    if (!root_node)
        root_node = new Inode(superblock, 0, extBS_32.root_cluster, Inode::Dir, 0, 1, 0, 0, 1, 0, 0, 0, 0);
    return root_node;
}

Status FAT_drive::write_buf_to_file(SharedPointer<Dentry>& dentry, const void* buf, uint length)
{
    if (dentry->inode->type != Inode::File)
        Status::failure("Trying to write data on something which is not a file");
    // Resize file if necessary
    if (dentry->inode->size != length)
        TRY(resize(dentry, length));
    if (length == 0)
        return Status::success();

    ctx ctx{};
    uint entry_id;
    if ((entry_id = TRY(get_child_dir_entry_id(dentry->parent, dentry->name, ctx))) == ENTRY_NOT_FOUND)
        return Status::failure("couldn't find file");
    DirEntry* file_entry = &entries[entry_id];

    uint next_cluster = file_entry->first_cluster_addr();
    size_t wrote_bytes = 0;

    // Write file content cluster by cluster
    while (wrote_bytes < length && next_cluster < CLUSTER_MIN_EOC)
    {
        TRY(change_active_cluster(next_cluster, ctx, this->buf));

        // If there is more than ATA_SECTOR_SIZE bytes to write, use provided buffer, otherwise copy remaining data to buf
        // and use buf, as its size is ATA_SECTOR_SIZE
        auto b = this->buf;
        uint rem = length - wrote_bytes;
        if (rem >= FAT_SECTOR_SIZE)
            b = (char*)buf + wrote_bytes;
        else
            memcpy(b, (char*)buf + wrote_bytes, length - wrote_bytes);
        TRY(write_data_sectors(1, ctx.active_sector, b, ctx));

        uint num = rem < FAT_SECTOR_SIZE ? rem : FAT_SECTOR_SIZE;
        wrote_bytes += num;
        next_cluster = ctx.table_value;
    }

    return Status::success();
}

Status FAT_drive::resize(SharedPointer<Dentry>& dentry, uint new_size)
{
    ctx ctx{};

    uint entry_id;
    if ((entry_id = TRY(get_child_dir_entry_id(dentry->parent, dentry->name, ctx))) == ENTRY_NOT_FOUND)
        return Status::failure("%s: couldn't find file %s", __func__, dentry->name);

    uint current_file_size = dentry->inode->size;
    uint curr_num_data_sectors = (current_file_size + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
    uint new_num_data_sectors = (new_size + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;

    // Update file size on disk
    entries[entry_id].file_size = new_size;

    if (new_num_data_sectors < curr_num_data_sectors)
    {
        uint curr_sector = entries[entry_id].first_cluster_addr();

        // If new size is 0, there's no need for any cluster, so we must unset the beginning of the cluster chain
        if (new_size == 0)
        {
            entries[entry_id].first_cluster_high = 0;
            entries[entry_id].first_cluster_low = 0;
        }
        // Persist disk file size metadata modification (and cluster beginning change if one has been made)
        TRY(write_data_sectors(1, ctx.active_sector, this->buf, ctx));

        // Shorten cluster chain
        // 1 - Skip remaining entries
        for (uint i = 0; i < new_num_data_sectors; i++)
        {
            TRY(change_active_cluster(curr_sector, ctx, this->buf));
            curr_sector = ctx.table_value;
        }
        // 2 - Indicate EOC
        TRY(change_active_cluster(curr_sector, ctx, this->buf));
        uint next_sect = ctx.table_value;
        *(uint*)&FAT[ctx.FAT_entry_offset] = ctx.table_value = new_size == 0 ? 0 : CLUSTER_EOC;
        TRY(write_fat(ctx)); // Write new FAT // Update FAT on disk
        curr_sector = next_sect;
        // 3 - Clear the rest of the chain
        for (uint i = 0; i < curr_num_data_sectors - new_num_data_sectors - 1; i++)
        {
            TRY(change_active_cluster(curr_sector, ctx, this->buf));
            uint next_sector = *(uint*)&FAT[ctx.FAT_entry_offset];
            *(uint*)&FAT[ctx.FAT_entry_offset] = ctx.table_value = 0;
            TRY(write_fat(ctx)); // Write new FAT // Update FAT on disk
            curr_sector = next_sector;
        }
    }
    else if (new_num_data_sectors > curr_num_data_sectors)
    {
#define RESIZE_BIGGER_ERR_RET_FALSE(errmsg) {delete free_cluster_list; return Status::failure(errmsg);}

        // multiple sectors per cluster absolutely not accounted for
        // Get free clusters
        const uint num_added_clusters = new_num_data_sectors - curr_num_data_sectors;
        auto free_cluster_list = TRY(get_free_clusters(num_added_clusters));
        if (free_cluster_list == nullptr)
            RESIZE_BIGGER_ERR_RET_FALSE("Not enough free clusters")

        // Index of the first new cluster to be added in the chain. It is most of the time 0, except for when
        // cluster chain's beginning is set, in which case its registration in the chain is taken care of manually,
        // as it is considered to be already part of the chain.
        uint clusters_to_be_registered_start_idx = 0;

        // If file was empty, indicate chain's first sector
        if (curr_num_data_sectors == 0)
        {
            entries[entry_id].first_cluster_high = free_cluster_list[0] >> 16;
            entries[entry_id].first_cluster_low = free_cluster_list[0] & 0xFFFF;
            clusters_to_be_registered_start_idx = 1;
        }
        // Persist disk file size metadata modification (and cluster chain's first sector change if it's the case)
        TRY(write_data_sectors(1, ctx.active_sector, this->buf, ctx));

        // Lengthen cluster chain with free_cluster_list
        // 1 - Skip current cluster chain
        uint curr_sector = entries[entry_id].first_cluster_addr();
        if (curr_num_data_sectors > 0)
        {
            for (uint i = 0; i < curr_num_data_sectors - 1; i++)
            {
                TRY(change_active_cluster(curr_sector, ctx, this->buf));
                curr_sector = ctx.table_value;
            }
        }
        // 2 - Add new chain entries
        for (uint i = clusters_to_be_registered_start_idx; i < num_added_clusters; i++)
        {
            TRY(change_active_cluster(curr_sector, ctx, this->buf));
            *(uint*)&FAT[ctx.FAT_entry_offset] = ctx.table_value = free_cluster_list[i];
            TRY(write_fat(ctx)); // Write new FAT // Update FAT on disk
            curr_sector = free_cluster_list[i];
        }
        // 3- Indicate EOC
        TRY(change_active_cluster(curr_sector, ctx, this->buf));
        *(uint*)&FAT[ctx.FAT_entry_offset] = ctx.table_value = CLUSTER_EOC;
        TRY(write_fat(ctx)); // Write new FAT // Update FAT on disk

        delete[] free_cluster_list;
    }
    else
    {
        // Persist disk file size metadata modification
        TRY(write_data_sectors(1, ctx.active_sector, this->buf, ctx));
    }

    dentry->inode->size = new_size; // Update inode size
    return Status::success();
}

Status FAT_drive::getdents(const SharedPointer<Dentry>& dentry, void* buffer, size_t max_size, size_t* bytes_read,
                           uint& fd_off)

{
    uint parent_sector = dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    ctx ctx{};
    *bytes_read = 0;
    auto remaining_bytes = [&]() {return max_size - *bytes_read;};
    char* buf = (char*)buffer;

    uint curr_cluster = parent_cluster;
    off_t off = 0;
    do
    {
        // ~= cd wd
        TRY(change_active_cluster(curr_cluster, ctx, this->buf));

        TmpString prev_lfn(1);
        auto prev_is_lfn = [&prev_lfn]() {return **prev_lfn != '\0';};
        while (ctx.dir_entry_id * sizeof(DirEntry) < FAT_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free())
        {
            if (off >= fd_off)
            {
                if (const auto entry = entries + ctx.dir_entry_id; entry->is_LFN())
                    prev_lfn = ((LongDirEntry*)entry)->get_uglily_converted_utf8_name().concat(prev_lfn);
                else
                {
                    TmpString entry_name = prev_is_lfn() ? prev_lfn : entry->get_name();
                    const auto dirent_size = ALIGN_UP(offsetof(struct dirent, d_name) + strlen(*entry_name), sizeof(struct dirent));
                    if (remaining_bytes() < dirent_size)
                        return Status::success(); // No more room available in buffer, exit
                    if (strlen(*entry_name) > __MLIBC_NAME_MAX)
                        irrecoverable_error("%s: file name '%s' is too long to be supported by mlibc", __PRETTY_FUNCTION__, *entry_name);
                    dirent* dirent = (struct dirent*)buf;
                    dirent->d_ino = entry->get_inode();
                    dirent->d_off = off;
                    dirent->d_reclen = dirent_size;
                    dirent->d_type = entry->is_directory() ? DT_DIR : DT_REG;
                    strcpy(dirent->d_name, *entry_name);
                    *bytes_read += dirent_size;
                    buf += dirent_size;
                    prev_lfn = TmpString(1);
                }
                fd_off++;
            }

            off++;
            ctx.dir_entry_id++;
        }
        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    return Status::success();
}

inline bool DirEntry::is_directory() const
{
    return attrs & DIRECTORY;
}

inline uint32_t DirEntry::first_cluster_addr() const
{
    return first_cluster_high << 16 | first_cluster_low;
}

char* DirEntry::get_extension() const
{
    char* extension = (char*)calloc(4, 1);
    uint dot_pos = 0;
    const TmpString file_name = get_name();
    const auto file_name_len = strlen(*file_name);
    while (dot_pos < file_name_len && (*file_name)[dot_pos] != '.')
        dot_pos++;
    if (file_name_len != dot_pos) // If file has an extension
        memcpy(extension, *file_name + dot_pos + 1, file_name_len - dot_pos - 1);

    return extension;
}

uint32_t DirEntry::get_inode() const
{
    return first_cluster_addr();
}

inline bool DirEntry::is_free() const
{
    return !name[0];
}

inline bool DirEntry::is_unused() const
{
    return (unsigned char)name[0] == 0xE5;
}

inline bool DirEntry::is_LFN() const
{
    return attrs & LFN;
}

DirEntry::DirEntry(const char* name, uint8_t attrs, uint32_t first_cluster_addr, uint32_t fileSize,
                   uint8_t creationTimeTenth, int16_t creationTime, int16_t creationDate, int16_t lastAccessDate,
                   uint16_t writeTime, uint16_t writeDate) : attrs(attrs),
                                                             creation_time_tenth(
                                                                 creationTimeTenth),
                                                             creation_time(creationTime),
                                                             creation_date(creationDate),
                                                             last_access_date(
                                                                 lastAccessDate),
                                                             first_cluster_high((first_cluster_addr >> 16) & 0xFFFF),
                                                             write_time(writeTime),
                                                             write_date(writeDate),
                                                             first_cluster_low(first_cluster_addr & 0xFFFF),
                                                             file_size(fileSize)
{
    memset(this->name, NAME_PADDING_BYTE, 10);
    if (!name) // Dummy constructor with no arguments call this constructor with nullptr as name
        return;
    if (strlen(name) > 8)
        irrecoverable_error("%s: file name '%s' too long, requires LFN support. Max supported length is 8", __func__, name);
    if (attrs & DIRECTORY)
        strcpy(this->name, name);
    else
    {
        uint l = strlen(name);
        uint i = l - 1;
        for (; i < DIR_ENTRY_NAME_LEN && name[i] != '.'; i--)
            this->name[10 - (l - 1 - i)] = name[i];
        if (i == (uint)-1)
        {
            memset(this->name, NAME_PADDING_BYTE, DIR_ENTRY_NAME_LEN);
            i = l;
        }
        for (uint j = 0; j < i; j++)
            this->name[j] = name[j];
    }
}

TmpString DirEntry::get_name() const
{
    TmpString n(DIR_ENTRY_NAME_LEN + 2); // One for dot, one for '\0'
    memset(*n, 0, DIR_ENTRY_NAME_LEN + 2);
    uint i = 0;
    for (int j = 0; j < 8; ++j)
    {
        if (name[j] == NAME_PADDING_BYTE)
            break;
        (*n)[i++] = name[j];
    }
    // If entry is a file, and it has an extension, add .
    if (!is_directory() && name[8] != NAME_PADDING_BYTE)
        (*n)[i++] = '.';
    for (int j = 8; j < 11; ++j)
    {
        if (name[j] == NAME_PADDING_BYTE)
            break;
        (*n)[i++] = name[j];
    }

    // Convert names to lowercase
    for (int j = 0; j < 12; ++j)
    {
        if ((*n)[j] >= 'A' && (*n)[j] <= 'Z')
            (*n)[j] -= 'A' - 'a';
    }

    return n;
}
