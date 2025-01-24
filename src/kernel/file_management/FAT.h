#ifndef BREBOS_FAT_H
#define BREBOS_FAT_H

#include "kstddef.h"
#include "kstdint.h"
#include "FS.h"
#include "dentry.h"

// https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
// https://wiki.osdev.org/FAT

#define FIRST_SECTOR_OF_CLUSTER(cluster, sectors_per_cluster, first_data_sector) (((cluster - 2) * sectors_per_cluster) + first_data_sector)

#define READONLY  1
#define HIDDEN    (1 << 1)
#define SYSTEM    (1 << 2)
#define VolumeID  (1 << 3)
#define DIRECTORY (1 << 4)
#define ARCHIVE   (1 << 5)
#define LFN (READONLY | HIDDEN | SYSTEM | VolumeID)

#define CLUSTER_EOC 0x0FFFFFFF
#define CLUSTER_MIN_EOC 0x0FFFFFF8
#define BAD_SECTOR 0x0FFFFFF7
#define LAST_LONG_ENTRY 0x40

#define NAME_PADDING_BYTE 0x20
#define DIR_ENTRY_NAME_LEN 11

enum FAT_type
{
	ExFAT,
	FAT12,
	FAT16,
	FAT32
};

// Bios Parameter BLOCK (aka BPB)
typedef struct fat_BS
{
	unsigned char bootjmp[3]; // Jump instruction to boot code
	unsigned char oem_name[8];
	unsigned short bytes_per_sector;
	unsigned char sectors_per_cluster;
	unsigned short reserved_sector_count;
	unsigned char table_count;
	unsigned short root_entry_count;
	unsigned short total_sectors_16; // Is 0 in FAT32
	unsigned char media_type;
	unsigned short table_size_16;
	unsigned short sectors_per_track;
	unsigned short head_side_count;
	uint hidden_sector_count; // Num sectors before the partition containing the FAT volume
	uint total_sectors_32;

	// this will be cast to it's specific type once the driver actually knows what type of FAT this is.
	unsigned char extended_section[54];
}__attribute__((packed)) fat_BS_t;

typedef struct fat_extBS_32
{
	uint table_size_32;
	unsigned short extended_flags;
	unsigned short fat_version;
	uint root_cluster;
	unsigned short fat_info;
	unsigned short backup_BS_sector; // Optional sector index in reserved area of a copy of BPB
	unsigned char reserved_0[12];
	unsigned char drive_number;
	unsigned char reserved_1;
	unsigned char boot_signature;
	uint volume_id;
	unsigned char volume_label[11];
	unsigned char fat_type_label[8];
}__attribute__((packed)) fat_extBS_32_t;

class __attribute__((packed)) LongDirEntry
{
	static char* utf16_to_utf8_cautionless_cast(const char* str, uint len);

public:
	uint8_t order; // Order of this entry in LFN chain
	char name1[10]; // Chars[1-5] of name in UTF16
	uint8_t attr; // == LFN
	uint8_t type;
	uint8_t checksum;
	char name2[12];
	uint16_t first_cluster_low;
	char name3[4];

	[[nodiscard]] char* get_uglily_converted_utf8_name() const;

	[[nodiscard]] bool is_EOF() const;
};

class __attribute__((packed)) DirEntry
{
public:
	char name[DIR_ENTRY_NAME_LEN];
	uint8_t attrs;
	const uint8_t NTRes = 0;
	uint8_t creation_time_tenth;
	int16_t creation_time;
	int16_t creation_date;
	int16_t last_access_date;
	uint16_t first_cluster_high;
	uint16_t write_time;
	uint16_t write_date;
	uint16_t first_cluster_low;
	uint32_t file_size;

	DirEntry(const char* name, uint8_t attrs, uint32_t first_cluster_addr, uint32_t fileSize,
	         uint8_t creationTimeTenth = -1,
	         int16_t creationTime = -1, int16_t creationDate = -1, int16_t lastAccessDate = -1,
	         uint16_t writeTime = -1, uint16_t writeDate = -1);

	[[nodiscard]] bool is_directory() const;

	[[nodiscard]] bool is_LFN() const;

	[[nodiscard]] bool is_free() const;

	[[nodiscard]] bool is_unused() const;

	[[nodiscard]] char* get_name() const;

	uint32_t first_cluster_addr();
};

struct directory
{
	uint32_t cluster;
	DirEntry* entries;
	uint32_t num_entries;
};

/* FAT32 ATA drive handler */
class FAT_drive : public FS
{
	static FAT_drive* drives[4];

	const fat_BS_t bs; // BPB
	const fat_extBS_32_t extBS_32; // Extended BPB

	const uint total_sectors;
	const uint fat_size;
	const uint root_dir_sectors;
	const uint first_data_sector;
	const uint first_fat_sector;
	const uint data_sectors;
	const uint total_clusters;

	char* buf; // Buffer used to browse directories entries
	DirEntry* entries; // Pointer to buf to read directories entries in it
	unsigned char* FAT; // Buffer to store FAT
	const unsigned char id; // Drive IDE ID
	const uint FAT_Buf_size; // Size of buf

	explicit FAT_drive(unsigned char id, fat_BS_t* bs);

	/**Splits a string based on '/' separator
	 *
	 * @param str string to split
	 * @param num_tokens returned value indicating number of tokens
	 * @return token list
	 */
	static const char** split_at_slashes(const char* str, uint* num_tokens);

	uint get_free_cluster(uint num_FAT_entries);

	/**Set environment to target a certain cluster
	 *
	 * @param new_active_cluster cluster we want to explore. Updates provided environment variables.
	 * @return boolean indicating whether the operation succeeded
	 */
	bool
	change_active_cluster(uint new_active_cluster, uint& active_cluster, uint& active_sector,
	                      uint& FAT_entry_offset, uint& FAT_offset, uint& FAT_sector, uint& table_value,
	                      uint& dir_entry_id);

	/** Browses file tree up to the directory containing the folder designed by path.
	 * Updates provided environment variables.
	 *
	 * @param path list of directories
	 * @param num_tokens number of directories
	 * @return boolean indicating whether the dir was found
	 */
	bool browse_to_folder_parent(const char** path, uint num_tokens, uint& active_cluster, uint& active_sector,
	                             uint& FAT_entry_offset, uint& FAT_offset, uint& FAT_sector, uint& table_value,
	                             uint& dir_entry_id);

	static FAT_drive* from_drive(unsigned char drive);

	Dentry* get_child_entry(Dentry& parent_dentry, const char* name) override;

public:
	bool touch(const Dentry& parent_dentry, const char* entry_name) override;

	bool mkdir(const Dentry& parent_dentry, const char* entry_name) override;

	bool ls(const Dentry& dentry) override;

	[[nodiscard]] static bool drive_present(uint drive_id);

	static void init();

	static void shutdown();

	void* load_file_to_buf(const char* file_name, Dentry* parent_dentry, uint offset, uint length) override;

	~FAT_drive() override;

	Inode* get_root_node() override;
};


#endif //BREBOS_FAT_H
