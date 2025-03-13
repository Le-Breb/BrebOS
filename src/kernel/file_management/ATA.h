#ifndef BREBOS_ATA_H
#define BREBOS_ATA_H

// Most of the code comes from https://wiki.osdev.org/PCI_IDE_Controller

#pragma region defines

#include <kstddef.h>

#define BAR0 (0x1F0) // start of the I/O ports used by the primary channel.
#define BAR1 (0x3F6) // start of the I/O ports which control the primary channel.
#define BAR2 (0x170) // start of the I/O ports used by secondary channel.
#define BAR3 (0x376) // start of the I/O ports which control secondary channel.
#define BAR4 (0x000) // start of 8 I/O ports controls the primary channel's Bus Master IDE.
#define BAR4P8 (BAR4 + 8) // Base of 8 I/O ports controls secondary channel's Bus Master IDE.

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Index
#define ATA_SR_ERR     0x01    // Error

#define ATA_ER_BBK      0x80    // Bad block
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // Media changed
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // Media change request
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA        0x00
#define IDE_ATAPI      0x01

#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Channels:
#define      ATA_PRIMARY      0x00
#define      ATA_SECONDARY    0x01

// Directions:
#define      ATA_READ      0x00
#define      ATA_WRITE     0x01

#define ATA_SECTOR_SIZE 512
#pragma endregion

typedef struct
{
	unsigned short base; // I/O Base.
	unsigned short ctrl; // Control Base
	unsigned short bmide; // Bus Master IDE
	unsigned char nIEN; // nIEN (No Interrupt);
} IDEChannelRegisters;

typedef struct
{
	unsigned char Reserved; // 0 (Empty) or 1 (This Drive really exists).
	unsigned char Channel; // 0 (Primary Channel) or 1 (Secondary Channel).
	unsigned char Drive; // 0 (Master Drive) or 1 (Slave Drive).
	unsigned short Type; // 0: ATA, 1:ATAPI.
	unsigned short Signature; // Drive Signature
	unsigned short Capabilities; // Features.
	uint CommandSets; // Command Sets Supported.
	uint Size; // Size in Sectors.
	unsigned char Model[41]; // Model in string.
} ide_device;

class FAT_drive;

class IDE
{
	friend class ATA;

	friend class FAT_drive;

	static IDEChannelRegisters channels[2];
	static volatile unsigned char irq_invoked;
	static ide_device devices[4];

	static void init();

	static unsigned char read(unsigned char channel, unsigned char reg);

	static void write(unsigned char channel, unsigned char reg, unsigned char data);

	static void read_buffer(unsigned char channel, unsigned char reg, void* buffer,
	                        uint quads);

	static unsigned char polling(unsigned char channel, uint advanced_check);

	static unsigned char print_error(uint drive, unsigned char err);
};

class ATA
{
	friend class FAT_drive;

	/**Read/Write an ATA device
	 *
	 * @param direction read = 0, write = 1
	 * @param drive drive number, [0-3]
	 * @param lba linear base address where operation will be performed
	 * @param numsects number of sectors. 0 means 256
	 * @param selector segment selector
	 * @param edi segment offset, ie buffer address
	 * @return
	 */
	static unsigned char access(unsigned char direction, unsigned char drive, uint lba,
	                            unsigned char numsects, unsigned short selector, uint edi);

	static void init();

public:
	static int read_sectors(unsigned char drive, unsigned char numsects, uint lba,
	                        unsigned short es, uint edi);

	static int write_sectors(unsigned char drive, unsigned char numsects, uint lba,
	                         unsigned short es, uint edi);

	static uint get_drive_size(unsigned char drive);

	static bool drive_present(unsigned char drive);
};


#endif //BREBOS_ATA_H
