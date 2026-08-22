#include "FS.h"

#include "ATA_Drive.h"
#include "Disk.h"
#include "FAT.h"
#include "USB_Drive.h"
#include "../../../bootloader/ATA.h"
#include "USB/SCSI.h"
#include "USB/USB.h"

FS::FS(blksize_t block_size, BlockDevice* dev) : superblock(nullptr), dev(dev), block_size(block_size)
{
}

bool FS::enumerate_block_device(BlockDevice* block_device)
{
	bool device_used = false;

	auto partitionning_info_res = Disk::get_partitioning_info(block_device);
	if (!partitionning_info_res.is_ok())
	{
		printf_warn("Failed to get disk info for USB device %u:%u. Excluding it of FS enumeration. Reason: %s",
				   block_device->get_major(), block_device->get_minor(), partitionning_info_res.err().what());
		return false;
	}

	const auto [partition_scheme, partitions] = std::move(partitionning_info_res).expect();
	for (const auto partition : partitions)
	{
		if (auto FAT = FAT::from_block_device(block_device, partition.lba_first); FAT.is_ok())
		{
			// No error occurred, but we still need to check whether something was returned: maybe this is not a FAT FS
			if (const auto fs = std::move(FAT).expect())
			{
				fs_list->add(fs);
				device_used = true;
			}
		}
		else // Error occurred while parsing FAT FS
			printf_warn("Failed to parse FAT file system on device %u:%u partition starting at LBA %u. Excluding it of FS enumeration. Reason: %s",
						block_device->get_major(), block_device->get_minor(), partition.lba_first, FAT.err().what());
	}

	return device_used;
}

FS::~FS() = default;

list<FS*>* FS::fs_list = nullptr;

void FS::init()
{
	fs_list = new list<FS*>();

	for (auto& usb_device : USB::get_mass_storage_devices())
	{
		auto read_cap_res = SCSI::send_read_capacity_10(&usb_device);
		if (!read_cap_res.is_ok())
		{
			printf_warn("Failed to get capacity for USB device %u:%u. Excluding it of FS enumeration. Reason: %s",
						usb_device.device->get_slot(), usb_device.interface_number, read_cap_res.err().what());
			continue;
		}
		const auto [last_lba, block_length] = std::move(read_cap_res).expect();
		const uint64_t total_size = static_cast<uint64_t>(last_lba + 1) * block_length;;
		const auto usb_dev = new USB_Drive(
									FAT_SECTOR_SIZE,
										MKDEV(DEV_USB_MASS_STORAGE_MAJOR, usb_device.interface_number),
										    total_size,
										    &usb_device);
		if (!enumerate_block_device(usb_dev))
			delete usb_dev;
	}

	for (uint i = 0; i < 4; ++i)
	{
		const auto total_size = ATA::get_drive_size(i);
		if (total_size == 0)
			continue; // Drive not found, skip
		const auto ata_dev = new ATA_Drive(FAT_SECTOR_SIZE, MKDEV(DEV_ATA_PRIMARY_MASTER_MAJOR, i), total_size, i);
		if (!enumerate_block_device(ata_dev))
			delete ata_dev;
	}
}

Result<void*> FS::load_file_to_buf(const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset,
                                   uint length, uint& loaded_bytes)
{
	// If reading an empty file, just return a dummy vector without solicitation of the disk
	if (length == 0)
		return make_ok<void*>(new char[1]);

	auto buf = new char[length];

	const auto res = load_file_to_buf(buf, file_name, parent_dentry, offset, length, loaded_bytes);
	if (res.is_ok())
		return make_ok((void*)buf);
	delete[] buf;
	return res;
}

blksize_t FS::get_block_size() const
{
	return block_size;
}

BlockDevice* FS::get_device() const
{
	return dev;
}
