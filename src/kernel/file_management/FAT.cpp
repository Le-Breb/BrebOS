#include "FAT.h"
#include <kstddef.h>
#include "ATA.h"
#include <kstring.h>
#include "superblock.h"
#include "dentry.h"
#include "../core/fb.h"
#include "../core/memory.h"
#include "../utils/comparison.h"

FAT_drive* FAT_drive::drives[] = {};

#define ES 0x10

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

char* LongDirEntry::utf16_to_utf8_cautionless_cast(const char* str, uint len)
{
    char* res = new char[len / 2 + 1];
    for (uint i = 0; i < len / 2; ++i)
    {
        res[i] = str[i * 2];
        if (str[i * 2] == '\0' && str[i * 2 + 1] == '\0')
        {
            res[i] = '\0';
            return res;
        }
    }
    res[len / 2] = '\0';

    return res;
}

char* LongDirEntry::get_uglily_converted_utf8_name() const
{
    char* n1 = utf16_to_utf8_cautionless_cast(name1, 10);
    char* n2 = strlen(n1) < 5 ? nullptr : utf16_to_utf8_cautionless_cast(name2, 12);
    char* n3 = (n2 && strlen(n2) < 6) ? nullptr : utf16_to_utf8_cautionless_cast(name3, 4);

    char* full_name = new char[10 + 12 + 4 + 1];
    memset(full_name, 0, 10 + 12 + 4 + 1);
    uint p = 0, gp = 0;
    while (p < 10 && n1[p])
        full_name[gp++] = n1[p++];
    if (n2)
    {
        p = 0;
        while (p < 12 && n2[p])
            full_name[gp++] = n2[p++];
    }
    if (n3)
    {
        p = 0;
        while (p < 4 && n3[p])
            full_name[gp++] = n3[p++];
    }

    delete[] n1;
    delete[] n2;
    delete[] n3;

    return full_name;
}

bool LongDirEntry::is_EOF() const
{
    return order & LAST_LONG_ENTRY;
}

FAT_drive* FAT_drive::from_drive(unsigned char drive, uint major)
{
    unsigned char buf[ATA_SECTOR_SIZE];
    if (ATA::read_sectors(drive, 1, 0, ES, (uint)buf))
        return nullptr;
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
        return nullptr;

    return new FAT_drive(drive, fat_boot, major);
}

FAT_drive::FAT_drive(unsigned char id, fat_BS_t* bs, uint major) : FS(ATA_SECTOR_SIZE,
                                                                            (major & 0xFF) << 8 | (id & 0xFF)),
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
                                                                   id(id)
{
    FAT = new unsigned char[ATA_SECTOR_SIZE];
    buf = new char[ATA_SECTOR_SIZE];
    entries = (DirEntry*)buf;

    if (bs->sectors_per_cluster != 1)
        printf_error("FAT driver only supports 1 sector per cluster, current value: %d.\n"
                     "(drive ID: %d). Drive accesses could misbehave.", id, bs->sectors_per_cluster);

    if (bs->bytes_per_sector != ATA_SECTOR_SIZE)
        printf_error("FAT driver only supports %d bytes per sector, current value: %d.\n"
                     "(drive ID: %d). Drive accesses could misbehave.", ATA_SECTOR_SIZE, bs->bytes_per_sector, id);
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

uint* FAT_drive::get_free_clusters(size_t n) const
{
    auto free_cluster_list = new uint[n];
    size_t free_clusters_found = 0;
    size_t num_fat_entries_per_sector = ATA_SECTOR_SIZE / sizeof(uint32_t);
    for (uint sector = first_fat_sector; sector < total_sectors; ++sector)
    {
        // Get FAT sector
        uint32_t fat_buf[num_fat_entries_per_sector];
        if (ATA::read_sectors(id, 1, sector, ES, (uint)fat_buf))
            ERR_RET_NULL("Drive read error")
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
                    return free_cluster_list;
            }
    }

    delete[] free_cluster_list;
    return nullptr;
}

FAT_drive::~FAT_drive()
{
    delete FAT;
    delete buf;

    fs_list->remove(this);
}

bool FAT_drive::change_active_cluster(uint new_active_cluster, ctx& ctx, void* buffer)
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
        if (ATA::read_sectors(id, 1, ctx.active_sector, ES, (uint)buffer))
            ERR_RET_FALSE("Drive read error")

        ctx.buffer_updated = false;
    }

    // Read FAT sector
    uint FAT_offset = ctx.active_cluster * sizeof(uint32_t);
    uint new_FAT_sector = first_fat_sector + (FAT_offset / ATA_SECTOR_SIZE);
    ctx.FAT_entry_offset = FAT_offset % ATA_SECTOR_SIZE;
    if (new_FAT_sector != ctx.FAT_sector)
    {
        ctx.FAT_sector = new_FAT_sector;
        if (ATA::read_sectors(id, 1, ctx.FAT_sector, ES, (uint)FAT))
            ERR_RET_FALSE("Drive read error")
    }

    ctx.table_value = *(uint*)&FAT[ctx.FAT_entry_offset];
    /*if (fat32) */
    ctx.table_value &= 0x0FFFFFFF;
    ctx.dir_entry_id = 0;

    return true;
}

void FAT_drive::init()
{
    ATA::init();
    for (uint i = 0; i < 4; ++i)
        drives[i] = ATA::drive_present(i) && IDE::devices[i].Type == IDE_ATA
                        ? from_drive(i, DEV_ATA_PRIMARY_MASTER_MAJOR)
                        : nullptr;

    // Register FS
    for (auto& drive : drives)
    {
        if (!drive)
            continue;
        fs_list->add(drive);
    }
}

void FAT_drive::shutdown()
{
    for (auto& drive : drives)
        delete drive;
}

bool FAT_drive::load_file_to_buf(void* buf, const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset, uint length, uint& loaded_bytes)
{
    ctx ctx{};
    uint file_entry_id;
    if ((file_entry_id = get_child_dir_entry_id(parent_dentry, file_name, ctx)) == ENTRY_NOT_FOUND)
        ERR_RET_FALSE("File not found");
    DirEntry* file_entry = &entries[file_entry_id];
    uint next_cluster = file_entry->first_cluster_addr();
    if (offset + length > file_entry->file_size)
        ERR_RET_FALSE("Offset + length exceeds file size");

    // Skip clusters until we reach the offset
    uint n_offset_clusters = offset / ATA_SECTOR_SIZE;
    uint cluster_offset = offset - n_offset_clusters * ATA_SECTOR_SIZE;
    uint co;
    for (co = 0; co < n_offset_clusters && next_cluster < CLUSTER_MIN_EOC; co++)
    {
        if (!change_active_cluster(next_cluster, ctx, nullptr))
            ERR_RET_FALSE("Disk read error");
        next_cluster = ctx.table_value;
    }
    if (co < n_offset_clusters)
        ERR_RET_FALSE("Offset exceeds file size");

    // If offset is 0 and length is greater than a sector, we can read the entire sector directly
    if (cluster_offset == 0 && length >= ATA_SECTOR_SIZE)
    {
        if (!change_active_cluster(next_cluster, ctx, buf))
            ERR_RET_FALSE("Disk read error");
        loaded_bytes = ATA_SECTOR_SIZE;
    }
    else // If offset is not 0, we need to load the sector and copy the meaningful data to buf
    {
        if (!change_active_cluster(next_cluster, ctx, this->buf))
            ERR_RET_FALSE("Disk read error");
        auto n = min(length, ATA_SECTOR_SIZE - cluster_offset);
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
        void* load_buf = rem < ATA_SECTOR_SIZE ? this->buf : b + loaded_bytes;
        if (!change_active_cluster(next_cluster, ctx, load_buf))
            ERR_RET_FALSE("Disk read error");
        if (rem < ATA_SECTOR_SIZE) // Entire sector has been loaded to buf, copy meaningful data to b
            memcpy(b + loaded_bytes, this->buf, rem);
        loaded_bytes += rem > ATA_SECTOR_SIZE ? ATA_SECTOR_SIZE : rem;
        next_cluster = ctx.table_value;
    }

    return true;
}

uint FAT_drive::get_child_dir_entry_id(const SharedPointer<Dentry>& parent_dentry, const char* name, ctx& ctx)
{
    // Make sure path makes sense
    if (!name || name[0] == '/')
        return ENTRY_NOT_FOUND;

    uint parent_sector = parent_dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    uint curr_cluster = parent_cluster;

    // Skip used dir entries, aka files/folders inside wd
    do
    {
        // ~= cd wd
        if (!change_active_cluster(curr_cluster, ctx, this->buf))
            return ENTRY_NOT_FOUND;

        bool found_in_lfn = false;
        while (ctx.dir_entry_id * sizeof(DirEntry) < ATA_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free())
        {
            if (found_in_lfn) // File name matched in previous entry which is a fln entry referring to the current entry
                return ctx.dir_entry_id;
            bool lfn = entries[ctx.dir_entry_id].is_LFN();
            char* entry_name = lfn
                                   ? ((LongDirEntry*)&entries[ctx.dir_entry_id])->get_uglily_converted_utf8_name()
                                   : entries[ctx.dir_entry_id].get_name();

            bool match = !strcmp(entry_name, name);
            delete[] entry_name;
            if (match)
            {
                found_in_lfn = lfn;
                if (!lfn)
                    return ctx.dir_entry_id;
            }

            ctx.dir_entry_id++;
        }
        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    return ENTRY_NOT_FOUND;
}

SharedPointer<Dentry> FAT_drive::get_child_dentry(SharedPointer<Dentry>& parent_dentry, const char* name)
{
    uint entry_id;
    ctx ctx{};
    if ((entry_id = get_child_dir_entry_id(parent_dentry, name, ctx)) == ENTRY_NOT_FOUND)
        return nullptr;
    return dir_entry_to_dentry(entries[entry_id], parent_dentry, name);
}

SharedPointer<Dentry> FAT_drive::dir_entry_to_dentry(const DirEntry& dir_entry, SharedPointer<Dentry>& parent_dentry, const char* name)
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
            if (!change_active_cluster(curr_cluster, ctx, nullptr))
            {
                printf_error("Failed to change active cluster while getting child dentry");
                return nullptr;
            }
            curr_cluster = ctx.table_value;
            blocks++;
        } while (curr_cluster < CLUSTER_MIN_EOC);
    }
    auto inode = new Inode(superblock, dir_entry.file_size, dir_entry.first_cluster_addr(), inode_type,
                           dir_entry.first_cluster_addr(), 1, 0, 0, 0, blocks, 0, 0, 0);

    return new Dentry(inode, parent_dentry, name);
}

bool FAT_drive::write_fat(const ctx& ctx) const
{
    if (ATA::write_sectors(id, 1, ctx.FAT_sector, ES, (uint)FAT)) // Write new FAT
        return true;

    return false;
}

bool FAT_drive::write_data_sectors(uint numsects, uint lba, const void* buffer, ctx& ctx) const
{
    if (ATA::write_sectors(id, numsects, lba, ES, (uint)buffer))
        return true;

    // If buffer is this->buf, then we already have the new data in memory. Otherwise, on a call to
    // change_active_cluster with new_cluster being equal to ctx.current_cluster, we will read the new data from disk,
    // which we indicate here
    ctx.buffer_updated = buffer != this->buf;

    return false;
}

SharedPointer<Dentry> FAT_drive::touch(SharedPointer<Dentry>& parent_dentry, const char* entry_name)
{
    // Make sure path makes sense
    if (!entry_name || entry_name[0] == '/')
        return nullptr;

    uint parent_sector = parent_dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    uint curr_cluster = parent_cluster;
    ctx ctx{};

    do
    {
        // ~= cd wd
        if (!change_active_cluster(curr_cluster, ctx, this->buf))
            return nullptr;

        // Skip used dir entries, aka files/folders inside wd
        while (ctx.dir_entry_id * sizeof(DirEntry) < ATA_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free() &&
            strcmp(entries[ctx.dir_entry_id].get_name(), entry_name) != 0)
            ctx.dir_entry_id++;

        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    // No free entry in wd cluster
    if (ctx.dir_entry_id * sizeof(DirEntry) == bs.bytes_per_sector * bs.sectors_per_cluster)
        ERR_RET_NULL("Working directory cluster is full, cluster chain extension implementation is needed");

    // Get a free cluster for file content
    auto free_cluster_list = get_free_clusters(1);
    if (free_cluster_list == nullptr)
        ERR_RET_NULL("No free cluster found")
    uint file_content_cluster = free_cluster_list[0];
    delete free_cluster_list;

    // Write file entry
    DirEntry new_entry(entry_name, 0, file_content_cluster, 0);
    memcpy(&entries[ctx.dir_entry_id], &new_entry, sizeof(DirEntry));
    if (write_data_sectors(1, ctx.active_sector, buf, ctx))
        ERR_RET_NULL("Drive write error")

    // ~= cd inside file
    if (!change_active_cluster(file_content_cluster, ctx, this->buf))
        return nullptr;

    // Indicate that dir content cluster is the end of the cluster chain it belongs to
    *(uint*)&FAT[ctx.FAT_entry_offset] = CLUSTER_EOC;
    if (write_fat(ctx)) // Write new FAT
        ERR_RET_NULL("Drive write error")

    return dir_entry_to_dentry(new_entry, parent_dentry, entry_name);
}

SharedPointer<Dentry> FAT_drive::mkdir(SharedPointer<Dentry>& parent_dentry, const char* entry_name)
{
    uint l = strlen(entry_name);
    if (l >= 12 - 3)
        ERR_RET_NULL("Dir name requires LFN support")

    uint parent_sector = parent_dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    uint curr_cluster = parent_cluster;
    ctx ctx{};

    do
    {
        // ~= cd wd
        if (!change_active_cluster(curr_cluster, ctx, this->buf))
            return nullptr;

        // Skip used dir entries, aka files/folders inside wd
        while (ctx.dir_entry_id * sizeof(DirEntry) < ATA_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free())
            ctx.dir_entry_id++;

        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    // No free entry in wd cluster
    if (ctx.dir_entry_id * sizeof(DirEntry) == bs.bytes_per_sector * bs.sectors_per_cluster)
        ERR_RET_NULL("Working directory cluster is full, cluster chain extension implementation is needed");

    auto free_clusters_list = get_free_clusters(1);
    if (free_clusters_list == nullptr)
        ERR_RET_NULL("No free cluster found")
    uint dir_content_cluster = free_clusters_list[0];
    delete[] free_clusters_list;

    // Write new entry
    DirEntry new_entry(entry_name, DIRECTORY, dir_content_cluster, 0);
    memcpy(&entries[ctx.dir_entry_id], &new_entry, sizeof(DirEntry));
    if (write_data_sectors(1, ctx.active_sector, buf, ctx))
        ERR_RET_NULL("Drive write error")

    // ~= cd new directory
    if (!change_active_cluster(dir_content_cluster, ctx, this->buf))
        return nullptr;

    uint dir_content_sector = ctx.active_sector;

    // Indicate that dir content cluster is the end of the cluster chain it belongs to
    *(uint*)&FAT[ctx.FAT_entry_offset] = CLUSTER_EOC;
    if (write_fat(ctx)) // Write new FAT
        ERR_RET_NULL("Drive write error")

    // Note: I guess this is unnecessary since dir content cluster is supposed to be empty
    if (ATA::read_sectors(id, 1, dir_content_sector, ES, (uint)buf))
        ERR_RET_NULL("Drive read error")

    // Create dot and dot dot entries
    DirEntry dot_entry(".", DIRECTORY, dir_content_cluster, 0);
    DirEntry dot_dot_entry("..", DIRECTORY, parent_cluster, 0);
    memcpy(&entries[0], &dot_entry, sizeof(DirEntry));
    memcpy(&entries[1], &dot_dot_entry, sizeof(DirEntry));

    // Write them to disk
    if (write_data_sectors(1, dir_content_sector, buf, ctx))
        ERR_RET_NULL("Drive write error")

    return dir_entry_to_dentry(new_entry, parent_dentry, entry_name);
}

bool FAT_drive::ls(const SharedPointer<Dentry>& dentry, ls_printer printer)
{
    uint parent_sector = dentry->inode->lba;
    uint parent_cluster = parent_sector * bs.sectors_per_cluster;
    ctx ctx{};

    uint curr_cluster = parent_cluster;
    do
    {
        // ~= cd wd
        if (!change_active_cluster(curr_cluster, ctx, this->buf))
            return false;

        // Acts like a boolean to know if the previous entry was a LFN entry, and if so, contains the lfn entry name
        char* prev_is_lfn = nullptr;
        while (ctx.dir_entry_id * sizeof(DirEntry) < ATA_SECTOR_SIZE && !entries[ctx.dir_entry_id].is_free())
        {
            auto entry = entries + ctx.dir_entry_id;
            bool is_lfn = entry->is_LFN();
            if (is_lfn)
                prev_is_lfn = ((LongDirEntry*)entry)->get_uglily_converted_utf8_name();
            else
            {
                char* entry_name = prev_is_lfn ? prev_is_lfn : entry->get_name();
                SharedPointer<Dentry> null_parent = {nullptr};
                SharedPointer<Dentry> dir_dentry = dir_entry_to_dentry(*entry, null_parent, entry_name);
                printer(*dir_dentry);
                delete[] entry_name;
                prev_is_lfn = nullptr;
            }

            ctx.dir_entry_id++;
        }
        curr_cluster = ctx.table_value;
    } while (curr_cluster < CLUSTER_MIN_EOC);

    return true;
}

bool FAT_drive::drive_present(uint drive_id)
{
    return drives[drive_id];
}

Inode* FAT_drive::get_root_node()
{
    return root_node
               ? root_node
               : root_node = new Inode(superblock, 0, extBS_32.root_cluster, Inode::Dir, 0, 1, 0, 0, 1, 0, 0, 0, 0);
}

bool FAT_drive::write_buf_to_file(SharedPointer<Dentry>& dentry, const void* buf, uint length)
{
    if (dentry->inode->type != Inode::File)
        ERR_RET_FALSE("Trying to write data on something which is not a file")
    // Resize file if necessary
    if (dentry->inode->size != length)
        if (!resize(dentry, length))
            return false;

    ctx ctx{};
    uint entry_id;
    if ((entry_id = get_child_dir_entry_id(dentry->parent, dentry->name, ctx)) == ENTRY_NOT_FOUND)
        ERR_RET_FALSE("couldn't find file")
    DirEntry* file_entry = &entries[entry_id];

    uint next_cluster = file_entry->first_cluster_addr();
    size_t wrote_bytes = 0;

    // Write file content cluster by cluster
    while (wrote_bytes < length && next_cluster < CLUSTER_MIN_EOC)
    {
        if (!change_active_cluster(next_cluster, ctx, this->buf))
            return false;

        // If there is more than ATA_SECTOR_SIZE bytes to write, use provided buffer, otherwise copy remaining data to buf
        // and use buf, as its size is ATA_SECTOR_SIZE
        auto b = this->buf;
        uint rem = length - wrote_bytes;
        if (rem >= ATA_SECTOR_SIZE)
            b = (char*)buf + wrote_bytes;
        else
            memcpy(b, (char*)buf + wrote_bytes, length - wrote_bytes);
        if (write_data_sectors(1, ctx.active_sector, b, ctx))
            ERR_RET_FALSE("Drive write error")

        uint num = rem < ATA_SECTOR_SIZE ? rem : ATA_SECTOR_SIZE;
        wrote_bytes += num;
        next_cluster = ctx.table_value;
    }

    return true;
}

bool FAT_drive::resize(SharedPointer<Dentry>& dentry, uint new_size) // Todo: handle files larger than new_size
{
    ctx ctx{};

    uint entry_id;
    if ((entry_id = get_child_dir_entry_id(dentry->parent, dentry->name, ctx)) == ENTRY_NOT_FOUND)
    {
        printf_error("%s: couldn't find file %s", __func__, dentry->name);
        return false;
    }

    // multiple sectors per cluster absolutely not accounted for
    // Get free clusters
    uint num_data_sectors = (new_size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
    auto free_cluster_list = get_free_clusters(num_data_sectors);
    if (free_cluster_list == nullptr)
        ERR_RET_FALSE("Not enough free clusters")

    // Update file entry
    entries[entry_id].file_size = new_size;
    entries[entry_id].first_cluster_high = free_cluster_list[0] >> 16;
    entries[entry_id].first_cluster_low = free_cluster_list[0] & 0xFFFF;
    if (write_data_sectors(1, ctx.active_sector, this->buf, ctx))
        ERR_RET_FALSE("Drive write error")

    // Update FAT
    for (uint i = 0; i < num_data_sectors - 1; i++)
    {
        if (!change_active_cluster(free_cluster_list[i], ctx, this->buf))
            ERR_RET_FALSE("drive read error")

        *(uint*)&FAT[ctx.FAT_entry_offset] = free_cluster_list[i + 1];
        if (write_fat(ctx)) // Update FAT on disk
            ERR_RET_FALSE("drive write error")
    }

    // Update last cluster
    if (!change_active_cluster(free_cluster_list[num_data_sectors - 1], ctx, this->buf))
        ERR_RET_FALSE("drive read error")
    *(uint*)&FAT[ctx.FAT_entry_offset] = CLUSTER_EOC;
    if (write_fat(ctx)) // Write new FAT // Update FAT on disk
        ERR_RET_FALSE("drive write error")

    delete[] free_cluster_list;
    dentry->inode->size = new_size; // Update inode size
    return true;
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
    const char* file_name = get_name();
    auto file_name_len = strlen(file_name);
    while (dot_pos < file_name_len && file_name[dot_pos] != '.')
        dot_pos++;
    if (file_name_len != dot_pos) // If file has an extension
        memcpy(extension, file_name + dot_pos + 1, file_name_len - dot_pos - 1);
    delete[] file_name;

    return extension;
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

char* DirEntry::get_name() const
{
    char* n = new char[DIR_ENTRY_NAME_LEN + 2]; // One for dot, one for '\0'
    memset(n, 0, DIR_ENTRY_NAME_LEN + 2);
    uint i = 0;
    for (int j = 0; j < 8; ++j)
    {
        if (name[j] == NAME_PADDING_BYTE)
            break;
        n[i++] = name[j];
    }
    // If entry is a file, and it has an extension, add .
    if (!is_directory() && name[8] != NAME_PADDING_BYTE)
        n[i++] = '.';
    for (int j = 8; j < 11; ++j)
    {
        if (name[j] == NAME_PADDING_BYTE)
            break;
        n[i++] = name[j];
    }

    // Convert names to lowercase
    for (int j = 0; j < 12; ++j)
    {
        if (n[j] >= 'A' && n[j] <= 'Z')
            n[j] -= 'A' - 'a';
    }

    return n;
}
