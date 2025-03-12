#include "FAT.h"
#include <kstddef.h>
#include "ATA.h"
#include <kstring.h>
#include <kstdio.h>
#include "superblock.h"
#include "dentry.h"
#include "../core/fb.h"

FAT_drive* FAT_drive::drives[] = {};

char* LongDirEntry::utf16_to_utf8_cautionless_cast(const char* str, uint len)
{
	char* res = new char[len / 2 + 1];
	for (uint i = 0; i < len / 2; ++i)
		res[i] = str[i * 2];
	res[len / 2] = '\0';

	return res;
}

char* LongDirEntry::get_uglily_converted_utf8_name() const
{
	char* n1 = utf16_to_utf8_cautionless_cast(name1, 10);
	char* n2 = utf16_to_utf8_cautionless_cast(name2, 12);
	char* n3 = utf16_to_utf8_cautionless_cast(name3, 4);

	char* full_name = new char[10 + 12 + 4 + 1];
	memset(full_name, 0, 10 + 12 + 4 + 1);
	uint p = 0, gp = 0;
	while (p < 10 && n1[p])
		full_name[gp++] = n1[p++];
	p = 0;
	while (p < 12 && n2[p])
		full_name[gp++] = n2[p++];
	while (p < 4 && n3[p])
		full_name[gp++] = n3[p++];

	delete[] n1;
	delete[] n2;
	delete[] n3;

	return full_name;
}

bool LongDirEntry::is_EOF() const
{
	return order & LAST_LONG_ENTRY;
}

FAT_drive* FAT_drive::from_drive(unsigned char drive)
{
	unsigned char buf[ATA_SECTOR_SIZE];
	if (ATA::read_sectors(drive, 1, 0, 0x10, (uint) buf))
		return nullptr;
	fat_BS_t* fat_boot = new fat_BS_t;
	memcpy(fat_boot, buf, sizeof(*fat_boot));

	fat_extBS_32_t* extBS_32 = (fat_extBS_32*) &fat_boot->extended_section;
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

	return new FAT_drive(drive, fat_boot);
}

FAT_drive::FAT_drive(unsigned char id, fat_BS_t* bs) : bs(*bs),
                                                       extBS_32(*(fat_extBS_32*) this->bs.extended_section),
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
                                                       id(id),
                                                       FAT_Buf_size(ATA_SECTOR_SIZE * extBS_32.table_size_32)
{
	FAT = new unsigned char[FAT_Buf_size];
	buf = new char[ATA_SECTOR_SIZE];
	entries = (DirEntry*) buf;
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
	return (const char**) res;
}

uint FAT_drive::get_free_cluster(uint num_FAT_entries)
{
	uint32_t* f = (uint32_t*) FAT;
	for (uint i = 2; i < num_FAT_entries; ++i) // Entries 0 and 1 are reserved
	{
		if (!f[i])
			return i;
	}

	return 0;
}

FAT_drive::~FAT_drive()
{
	delete FAT;
	delete buf;

	fs_list->remove(this);
}

bool FAT_drive::change_active_cluster(uint new_active_cluster, uint& active_cluster, uint& active_sector,
                                      uint& FAT_entry_offset, uint& FAT_offset, uint& FAT_sector, uint& table_value,
                                      uint& dir_entry_id)
{
	active_cluster = new_active_cluster;
	active_sector = FIRST_SECTOR_OF_CLUSTER(active_cluster, bs.sectors_per_cluster, first_data_sector);

	// Read data from drive
	if (ATA::read_sectors(id, 1, active_sector, 0x10, (uint) buf))
	{
		printf_error("Drive read error");
		return false;
	}

	FAT_offset = active_cluster * sizeof(uint32_t);
	FAT_sector = first_fat_sector + (FAT_offset / ATA_SECTOR_SIZE);
	FAT_entry_offset = FAT_offset % ATA_SECTOR_SIZE;
	if (ATA::read_sectors(id, extBS_32.table_size_32, FAT_sector, 0x10, (uint) FAT))
	{
		printf_error("Drive read error");
		return false;
	}

	table_value = *(uint*) &FAT[FAT_entry_offset];
	/*if (fat32) */
	table_value &= 0x0FFFFFFF;
	dir_entry_id = 0;

	return true;
}

bool FAT_drive::browse_to_folder_parent(const char** path, uint num_tokens, uint& active_cluster, uint& active_sector,
                                        uint& FAT_entry_offset, uint& FAT_offset, uint& FAT_sector, uint& table_value,
                                        uint& dir_entry_id)
{
	uint token_id = 0;

	if (!change_active_cluster(extBS_32.root_cluster, active_cluster, active_sector,
	                           FAT_entry_offset, FAT_offset, FAT_sector, table_value, dir_entry_id))
		return false;

	if (!num_tokens)
		return true;

	// Browse file tree to find working directory
	bool prev_is_lfn = false, found_in_lfn = false, found = false;
	while (!entries[dir_entry_id].is_free())
	{
		if (!(unsigned char) entries[dir_entry_id].is_unused()) // Skip unused entry
		{
			// Long directory entry
			if (entries[dir_entry_id].is_LFN())
			{
				LongDirEntry* l_entry = (LongDirEntry*) &entries[dir_entry_id]; // Cast entry
				// Ensure the long entries chain only contains one element
				if (!(l_entry->is_EOF()))
				{
					printf_error("chained long entries not supported");
					return false;
				}

				// Concatenate name1,2,3 from UTF16 to UTF8
				char* full_name = l_entry->get_uglily_converted_utf8_name();

				// Does the name match ?
				found_in_lfn = !strcmp(full_name, path[token_id]);
				prev_is_lfn = true;

				delete full_name;
			}
			else // Regular 8.3 entry
			{
				found = found_in_lfn;
				// No LFN is linked with this entry, then we compare its name with the expected token
				if (!found && !prev_is_lfn)
				{
					char* name = entries[dir_entry_id].get_name();
					found = !strcmp(name, path[token_id]);
					delete name;
				}
				// Token has been found either in linked LFN or in this 8.3 entry
				if (found)
				{
					token_id++; // Current token has been found, proceed to next one
					if (token_id == num_tokens)
						break; // We are at the right working directory, stop browsing file tree

					if (!(entries[dir_entry_id].is_directory()))
						return false; // Token is not a directory, abort

					// We need to explore subdirectories. To do so, read directory entry from drive
					if (!change_active_cluster(entries[dir_entry_id].first_cluster_addr(), active_sector,
					                           active_sector, FAT_entry_offset, FAT_offset, FAT_sector,
					                           table_value, dir_entry_id))
						return false;
				}
				prev_is_lfn = found = found_in_lfn = false;
			}
		}
		dir_entry_id++; // Current entry did not match token, proceed to next one

		// Reached end of sector, follow cluster chain
		if (dir_entry_id * sizeof(uint32_t) == ATA_SECTOR_SIZE)
		{
			if (table_value == BAD_SECTOR)
			{
				printf_error("Bad sector");
				return false;
			}
			// Did we reach the end of the chain ?
			if (table_value >= CLUSTER_MIN_EOC)
				break; // Yes, stop reading entries

			// Now update variables
			if (!change_active_cluster(table_value, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
			                           FAT_sector, table_value, dir_entry_id))
				return false;
		}
	}

	return found;
}

void FAT_drive::init()
{
	ATA::init();
	for (uint i = 0; i < 4; ++i)
		drives[i] = ATA::drive_present(i) && IDE::devices[i].Type == IDE_ATA ? FAT_drive::from_drive(i) : nullptr;

	// Register FS
	for (uint i = 0; i < 4; ++i)
	{
		if (!drives[i])
			continue;
		fs_list->add(drives[i]);
	}
}

void FAT_drive::shutdown()
{
	for (uint i = 0; i < 4; ++i)
		delete drives[i];
}

void* FAT_drive::load_file_to_buf(const char* file_name, Dentry* parent_dentry, uint offset, uint length)
{
	uint parent_sector = parent_dentry->inode->lba;
	uint parent_cluster = parent_sector * bs.sectors_per_cluster;
	uint active_cluster, active_sector, FAT_offset, FAT_sector, FAT_entry_offset, table_value, dir_entry_id;
	// ~= cd wd
	if (!change_active_cluster(parent_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
	                           FAT_sector, table_value, dir_entry_id))
		return nullptr;

	// Skip used dir entries, aka files/folders inside wd
	while (dir_entry_id * sizeof(DirEntry) < FAT_Buf_size && !entries[dir_entry_id].is_free() &&
	       strcmp(entries[dir_entry_id].get_name(), file_name) != 0)
		dir_entry_id++;

	if (dir_entry_id * sizeof(DirEntry) >= FAT_Buf_size || entries[dir_entry_id].is_free())
		return nullptr;

	// Be careful, this pointer losses its meaning when changing active_cluster
	DirEntry* file_entry = &entries[dir_entry_id];
	uint next_cluster = file_entry->first_cluster_addr();
	if (offset + length > file_entry->file_size)
		return nullptr;

	uint wrote_bytes = 0;
	char* b = new char[length];
	while (wrote_bytes < length && next_cluster != CLUSTER_EOC)
	{
		if (!change_active_cluster(next_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
		                           FAT_sector, table_value, dir_entry_id))
		{
			delete[] b;
			return nullptr;
		}
		uint rem = length - wrote_bytes;
		uint num = rem < ATA_SECTOR_SIZE ? rem : ATA_SECTOR_SIZE;
		memcpy(b + wrote_bytes, buf, num);
		wrote_bytes += num;
		next_cluster = table_value;
	}

	if (wrote_bytes == length)
		return b;

	delete[] b;
	return nullptr;
}


Dentry* FAT_drive::get_child_entry(Dentry& parent_dentry, const char* name)
{
	// Make sure path makes sense
	if (!name || name[0] == '/')
		return nullptr;

	uint parent_sector = parent_dentry.inode->lba;
	uint parent_cluster = parent_sector * bs.sectors_per_cluster;
	uint active_cluster, active_sector, FAT_offset, FAT_sector, FAT_entry_offset, table_value, dir_entry_id;
	// ~= cd wd
	if (!change_active_cluster(parent_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
	                           FAT_sector, table_value, dir_entry_id))
		return nullptr;

	// Skip used dir entries, aka files/folders inside wd
	bool found_in_lfn = false;
	while (dir_entry_id * sizeof(DirEntry) < FAT_Buf_size && !entries[dir_entry_id].is_free())
	{
		if (found_in_lfn) // File name matched in previous entry which is a fln entry referring to the current entry
			break;
		bool lfn = entries[dir_entry_id].is_LFN();
		char* entry_name = lfn
			                   ? ((LongDirEntry*) &entries[dir_entry_id])->get_uglily_converted_utf8_name()
			                   : entries[dir_entry_id].get_name();

		bool match = !strcmp(entry_name, name);
		delete[] entry_name;
		if (match)
		{
			found_in_lfn = lfn;
			if (!lfn)
				break;
		}

		dir_entry_id++;
	}

	if (dir_entry_id * sizeof(DirEntry) >= FAT_Buf_size || entries[dir_entry_id].is_free())
		return nullptr;

	// Be careful, this pointer losses its meaning when changing active_cluster
	DirEntry* file_entry = &entries[dir_entry_id];
	Inode::Type node_type = file_entry->attrs & DIRECTORY ? Inode::Dir : Inode::File;
	Inode* file_node = new Inode(superblock, file_entry->file_size, file_entry->first_cluster_addr(), node_type);

	return new Dentry(file_node, &parent_dentry, name);
}

bool FAT_drive::touch(const Dentry& parent_dentry, const char* entry_name)
{
	// Make sure path makes sense
	if (!entry_name || entry_name[0] == '/')
		return false;

	uint parent_sector = parent_dentry.inode->lba;
	uint parent_cluster = parent_sector * bs.sectors_per_cluster;
	uint active_cluster, active_sector, FAT_offset, FAT_sector, FAT_entry_offset, table_value, dir_entry_id;
	// ~= cd wd
	if (!change_active_cluster(parent_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
	                           FAT_sector, table_value, dir_entry_id))
		return false;

	// Skip used dir entries, aka files/folders inside wd
	while (dir_entry_id * sizeof(DirEntry) < FAT_Buf_size && !entries[dir_entry_id].is_free() &&
	       strcmp(entries[dir_entry_id].get_name(), entry_name) != 0)
		dir_entry_id++;

	// No free entry in wd cluster
	if (dir_entry_id * sizeof(DirEntry) == bs.bytes_per_sector * bs.sectors_per_cluster)
	{
		printf_error("Working directory cluster is full, chaining implementation is needed");
		return false;
	}

	// Get a free cluster for file content
	uint file_content_cluster = get_free_cluster(ATA_SECTOR_SIZE * bs.bytes_per_sector / sizeof(DirEntry));
	if (!file_content_cluster)
	{
		printf_error("No free cluster found");
		return false;
	}

	// Write file entry
	DirEntry new_entry(entry_name, 0, file_content_cluster, 0);
	memcpy(&entries[dir_entry_id], &new_entry, sizeof(DirEntry));
	if (ATA::write_sectors(id, 1, active_sector, 0x10, (uint) buf))
	{
		printf_error("Drive write error");
		return false;
	}

	// ~= cd inside file
	if (!change_active_cluster(file_content_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
	                           FAT_sector, table_value, dir_entry_id))
		return false;

	// Indicate that dir content cluster is the end of the cluster chain it belongs to
	*(uint*) &FAT[FAT_entry_offset] = CLUSTER_EOC;
	if (ATA::write_sectors(id, 1, FAT_sector, 0x10, (uint) FAT)) // Write new FAT
	{
		printf_error("drive write error");
		return false;
	}

	return true;
}

bool FAT_drive::mkdir(const Dentry& parent_dentry, const char* entry_name)

{
	uint l = strlen(entry_name);
	if (l >= 12 - 3)
	{
		printf_error("Dir name requires LFN support");
		return false;
	}

	uint parent_sector = parent_dentry.inode->lba;
	uint parent_cluster = parent_sector * bs.sectors_per_cluster;
	uint active_cluster, active_sector, FAT_offset, FAT_sector, FAT_entry_offset, table_value, dir_entry_id;
	// ~= cd wd
	if (!change_active_cluster(parent_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
	                           FAT_sector, table_value, dir_entry_id))
		return false;

	// Skip used dir entries, aka files/folders inside wd
	while (dir_entry_id * sizeof(DirEntry) < FAT_Buf_size && !entries[dir_entry_id].is_free())
		dir_entry_id++;

	// No free entry in wd cluster
	if (dir_entry_id * sizeof(DirEntry) == bs.bytes_per_sector * bs.sectors_per_cluster)
	{
		printf_error("Working directory cluster is full, chaining implementation is needed");
		return false;
	}

	uint dir_content_cluster = get_free_cluster(ATA_SECTOR_SIZE * extBS_32.table_size_32 *
	                                            bs.sectors_per_cluster * sizeof(DirEntry));
	if (!dir_content_cluster)
	{
		printf_error("no free cluster found");
		return false;
	}

	// Write new entry
	DirEntry new_entry(entry_name, DIRECTORY, dir_content_cluster, 0);
	memcpy(&entries[dir_entry_id], &new_entry, sizeof(DirEntry));
	if (ATA::write_sectors(id, 1, active_sector, 0x10, (uint) buf))
	{
		printf_error("Drive write error");
		return false;
	}

	// ~= cd new directory
	if (!change_active_cluster(dir_content_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
	                           FAT_sector, table_value, dir_entry_id))
		return false;

	uint dir_content_sector = active_sector;

	// Indicate that dir content cluster is the end of the cluster chain it belongs to
	*(uint*) &FAT[FAT_entry_offset] = CLUSTER_EOC;
	if (ATA::write_sectors(id, 1, FAT_sector, 0x10, (uint) FAT)) // Write new FAT
	{
		printf_error("drive write error");
		return false;
	}

	// Note: I guess this is unnecessary since dir content cluster is supposed to be empty
	if (ATA::read_sectors(id, 1, dir_content_sector, 0x10, (uint) buf))
	{
		printf_error("drive read error");
		return false;
	}

	// Create dot and dot dot entries
	DirEntry dot_entry(".", DIRECTORY, dir_content_cluster, 0);
	DirEntry dot_dot_entry("..", DIRECTORY, parent_sector, 0);
	memcpy(&entries[0], &dot_entry, sizeof(DirEntry));
	memcpy(&entries[1], &dot_dot_entry, sizeof(DirEntry));

	// Write them to disk
	if (ATA::write_sectors(id, 1, dir_content_sector, 0x10, (uint) buf))
	{
		printf_error("Drive write error");
		return false;
	}

	return true;
}

bool FAT_drive::ls(const Dentry& dentry)
{
	uint parent_sector = dentry.inode->lba;
	uint parent_cluster = parent_sector * bs.sectors_per_cluster;
	uint active_cluster, active_sector, FAT_offset, FAT_sector, FAT_entry_offset, table_value, dir_entry_id;
	// ~= cd wd
	if (!change_active_cluster(parent_cluster, active_cluster, active_sector, FAT_entry_offset, FAT_offset,
	                           FAT_sector, table_value, dir_entry_id))
		return false;

	// Skip used dir entries, aka files/folders inside wd
	bool prev_is_lfn = false;
	while (dir_entry_id * sizeof(DirEntry) < FAT_Buf_size && !entries[dir_entry_id].is_free())
	{
		auto entry = entries + dir_entry_id;
		bool is_lfn = entry->is_LFN();
		if (!prev_is_lfn)
		{
			char* entry_name = is_lfn ? ((LongDirEntry*)entry)->get_uglily_converted_utf8_name() : entry->get_name();
			printf("%s\n", entry_name);
			delete[] entry_name;
		}

		dir_entry_id++;
		prev_is_lfn = is_lfn;
	}

	return true;
}

bool FAT_drive::drive_present(uint drive_id)
{
	return drives[drive_id];
}

Inode* FAT_drive::get_root_node()
{
	return new Inode(superblock, 0, extBS_32.root_cluster, Inode::Dir);
}

bool DirEntry::is_directory() const
{
	return attrs & DIRECTORY;
}

uint32_t DirEntry::first_cluster_addr()
{
	return first_cluster_high << 16 | first_cluster_low;
}

bool DirEntry::is_free() const
{
	return !name[0];
}

bool DirEntry::is_unused() const
{
	return (unsigned char) name[0] == 0xE5;
}

bool DirEntry::is_LFN() const
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
	if (attrs & DIRECTORY)
		memcpy(this->name, name, strlen(name) + 1);
	else
	{
		uint l = strlen(name);
		uint i = l - 1;
		for (; i < DIR_ENTRY_NAME_LEN && name[i] != '.'; i--)
			this->name[10 - (l - 1 - i)] = name[i];
		if (i == (uint) -1)
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
	char* n = new char[12];
	memset(n, 0, 12);
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
