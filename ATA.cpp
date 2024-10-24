#include "ATA.h"
#include "IO.h"
#include "clib/stddef.h"
#include "clib/stdio.h"
#include "fb.h"

volatile unsigned char IDE::irq_invoked = 0;
IDEChannelRegisters IDE::channels[2] = {};
ide_device IDE::devices[4] = {};

void sleep([[maybe_unused]] uint ms)
{
	//Todo: Implement this
}

void IDE::init()
{
	static unsigned char buf[2048] = {0};
	int j, k, n = 0;

	// 1- Detect I/O Ports which interface IDE Controller:
	channels[ATA_PRIMARY  ].base  = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
	channels[ATA_PRIMARY  ].ctrl  = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
	channels[ATA_SECONDARY].base  = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
	channels[ATA_SECONDARY].ctrl  = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
	channels[ATA_PRIMARY  ].bmide = (BAR4 & 0xFFFFFFFC) + 0; // Bus Master IDE
	channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE

	// 2- Disable IRQs:
	write(ATA_PRIMARY  , ATA_REG_CONTROL, 2);
	write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

	// 3- Detect ATA-ATAPI Devices:
	for (int i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
		{
			unsigned char err = 0, type = IDE_ATA, status;
			devices[n].Reserved = 0; // Assuming that no drive here.

			// (I) Select Drive:
			write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive.
			sleep(1); // Wait 1ms for drive select to work.

			// (II) Send ATA Identify Command:
			write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			sleep(1); // This function should be implemented in your OS. which waits for 1 ms.
			// it is based on System Timer Device Driver.

			// (III) Polling:
			if (read(i, ATA_REG_STATUS) == 0) continue; // If Status = 0, No Device.

			while(1) {
				status = read(i, ATA_REG_STATUS);
				if ((status & ATA_SR_ERR)) {err = 1; break;} // If Err, Device is not ATA.
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break; // Everything is right.
			}

			// (IV) Probe for ATAPI Devices:
			if (err != 0) {
				unsigned char cl = read(i, ATA_REG_LBA1);
				unsigned char ch = read(i, ATA_REG_LBA2);

				if (cl == 0x14 && ch == 0xEB)
					type = IDE_ATAPI;
				else if (cl == 0x69 && ch == 0x96)
					type = IDE_ATAPI;
				else
					continue; // Unknown Type (may not be a device).

				write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				sleep(1);
			}

			// (V) Read Identification Space of the Device:
			read_buffer(i, ATA_REG_DATA, buf, 128);

			// (VI) Read Device Parameters:
			devices[n].Reserved     = 1;
			devices[n].Type         = type;
			devices[n].Channel      = i;
			devices[n].Drive        = j;
			devices[n].Signature    = *((unsigned short *)(buf + ATA_IDENT_DEVICETYPE));
			devices[n].Capabilities = *((unsigned short *)(buf + ATA_IDENT_CAPABILITIES));
			devices[n].CommandSets  = *((unsigned int *)(buf + ATA_IDENT_COMMANDSETS));

			// (VII) Get Size:
			if (devices[n].CommandSets & (1 << 26))
				// Device uses 48-Bit Addressing:
				devices[n].Size   = *((unsigned int *)(buf + ATA_IDENT_MAX_LBA_EXT));
			else
				// Device uses CHS or 28-bit Addressing:
				devices[n].Size   = *((unsigned int *)(buf + ATA_IDENT_MAX_LBA));

			// (VIII) String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
			for(k = 0; k < 40; k += 2) {
				devices[n].Model[k] = buf[ATA_IDENT_MODEL + k + 1];
				devices[n].Model[k + 1] = buf[ATA_IDENT_MODEL + k];}
			devices[n].Model[40] = 0; // Terminate String.

			n++;
		}

	// 4- Print Summary:
	for (int i = 0; i < 4; i++)
		if (devices[i].Reserved == 1)
		{
			printf(" Drive ");
			FB::set_fg(FB_LIGHTMAGENTA);
			printf("%d", i);
			FB::set_fg(FB_WHITE);
			printf(": %s Drive %dMB - %s\n",
				   (const char *[]){"ATA", "ATAPI"}[devices[i].Type],
					devices[i].Size / 1024 / 2,
					devices[i].Model);
		}
}

unsigned char IDE::read(unsigned char channel, unsigned char reg)
{
	unsigned char result;
	if (reg > 0x07 && reg < 0x0C)
		write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	if (reg < 0x08)
		result = inb(channels[channel].base + reg - 0x00);
	else if (reg < 0x0C)
		result = inb(channels[channel].base + reg - 0x06);
	else if (reg < 0x0E)
		result = inb(channels[channel].ctrl + reg - 0x0A);
	else if (reg < 0x16)
		result = inb(channels[channel].bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
	return result;
}

void IDE::write(unsigned char channel, unsigned char reg, unsigned char data)
{
	if (reg > 0x07 && reg < 0x0C)
		write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	if (reg < 0x08)
		outb(channels[channel].base + reg - 0x00, data);
	else if (reg < 0x0C)
		outb(channels[channel].base + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(channels[channel].ctrl + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(channels[channel].bmide + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

void IDE::read_buffer(unsigned char channel, unsigned char reg, void* buffer, unsigned int quads)
{
	/* WARNING: This code contains a serious bug. The inline assembly trashes ES and
	 *           ESP for all of the code the compiler generates between the inline
	 *           assembly blocks.
	 */
	if (reg > 0x07 && reg < 0x0C)
		write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	asm("pushw %es; movw %ds, %ax; movw %ax, %es");
	if (reg < 0x08)
		IO::insl(channels[channel].base + reg - 0x00, buffer, quads);
	else if (reg < 0x0C)
		IO::insl(channels[channel].base + reg - 0x06, buffer, quads);
	else if (reg < 0x0E)
		IO::insl(channels[channel].ctrl + reg - 0x0A, buffer, quads);
	else if (reg < 0x16)
		IO::insl(channels[channel].bmide + reg - 0x0E, buffer, quads);
	asm("popw %es;");
	if (reg > 0x07 && reg < 0x0C)
		write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

unsigned char IDE::polling(unsigned char channel, unsigned int advanced_check) {

	// (I) Delay 400 nanosecond for BSY to be set:
	// -------------------------------------------------
	for(int i = 0; i < 4; i++)
		read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times.

	// (II) Wait for BSY to be cleared:
	// -------------------------------------------------
	while (read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		; // Wait for BSY to be zero.

	if (advanced_check) {
		unsigned char state = read(channel, ATA_REG_STATUS); // Read Status Register.

		// (III) Check For Errors:
		// -------------------------------------------------
		if (state & ATA_SR_ERR)
			return 2; // Error.

		// (IV) Check If Device fault:
		// -------------------------------------------------
		if (state & ATA_SR_DF)
			return 1; // Device Fault.

		// (V) Check DRQ:
		// -------------------------------------------------
		// BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
		if ((state & ATA_SR_DRQ) == 0)
			return 3; // DRQ should be set

	}

	return 0; // No Error.
}

unsigned char IDE::print_error(unsigned int drive, unsigned char err) {
	if (err == 0)
		return err;

	printf("IDE:");
	if (err == 1) {printf("- Device Fault\n     "); err = 19;}
	else if (err == 2) {
		unsigned char st = read(devices[drive].Channel, ATA_REG_ERROR);
		if (st & ATA_ER_AMNF)   {printf("- No Address Mark Found\n     ");   err = 7;}
		if (st & ATA_ER_TK0NF)   {printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_ABRT)   {printf("- Command Aborted\n     ");      err = 20;}
		if (st & ATA_ER_MCR)   {printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_IDNF)   {printf("- ID mark not Found\n     ");      err = 21;}
		if (st & ATA_ER_MC)   {printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_UNC)   {printf("- Uncorrectable Data Error\n     ");   err = 22;}
		if (st & ATA_ER_BBK)   {printf("- Bad Sectors\n     ");       err = 13;}
	} else  if (err == 3)           {printf("- Reads Nothing\n     "); err = 23;}
	else  if (err == 4)  {printf("- ATAPI not supported\n     "); err = 8;}
	printf("- [%s %s] %s\n",
		   (const char *[]){"Primary", "Secondary"}[devices[drive].Channel], // Use the channel as an index into the array
			(const char *[]){"Master", "Slave"}[devices[drive].Drive], // Same as above, using the drive
	devices[drive].Model);

	return err;
}

unsigned char ATA::access(unsigned char direction, unsigned char drive, unsigned int lba, unsigned char numsects,
						  unsigned short selector, unsigned int edi)
{
	unsigned char lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */, dma /* 0: No DMA, 1: DMA */, cmd;
	unsigned char lba_io[6];
	unsigned int  channel      = IDE::devices[drive].Channel; // Read the Channel.
	unsigned int  slavebit      = IDE::devices[drive].Drive; // Read the Drive [Master/Slave]
	unsigned int  bus = IDE::channels[channel].base; // Bus Base, like 0x1F0 which is also data port.
	unsigned int  words      = 256; // Almost every ATA drive has a sector-size of 512-byte.
	unsigned short cyl, i;
	unsigned char head, sect, err;

	// Disable IRQs by setting bit 1 if the Control Register (nIEN bit). This affects master and slave of this channel
	IDE::write(channel, ATA_REG_CONTROL, IDE::channels[channel].nIEN = (IDE::irq_invoked = 0x0) + 0x02);

	// (I) Select one from LBA28, LBA48 or CHS;
	if (lba >= 0x10000000) { // Sure Drive should support LBA in this case, or you are
		// giving a wrong LBA.
		// LBA48:
		lba_mode  = 2;
		lba_io[0] = (lba & 0x000000FF) >> 0;
		lba_io[1] = (lba & 0x0000FF00) >> 8;
		lba_io[2] = (lba & 0x00FF0000) >> 16;
		lba_io[3] = (lba & 0xFF000000) >> 24;
		lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
		lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
		head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
	} else if (IDE::devices[drive].Capabilities & 0x200)  { // Drive supports LBA?
		// LBA28:
		lba_mode  = 1;
		lba_io[0] = (lba & 0x00000FF) >> 0;
		lba_io[1] = (lba & 0x000FF00) >> 8;
		lba_io[2] = (lba & 0x0FF0000) >> 16;
		lba_io[3] = 0; // These Registers are not used here.
		lba_io[4] = 0; // These Registers are not used here.
		lba_io[5] = 0; // These Registers are not used here.
		head      = (lba & 0xF000000) >> 24;
	} else {
		// CHS:
		lba_mode  = 0;
		sect      = (lba % 63) + 1;
		cyl       = (lba + 1  - sect) / (16 * 63);
		lba_io[0] = sect;
		lba_io[1] = (cyl >> 0) & 0xFF;
		lba_io[2] = (cyl >> 8) & 0xFF;
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		head      = (lba + 1  - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
	}

	// (II) See if drive supports DMA or not;
	dma = 0; // We don't support DMA

	// (III) Wait if the drive is busy;
	while (IDE::read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {}

	// (IV) Select Drive from the controller;
	/**HDDDEVSEL layout
	 * Bits 0 : 3: Head Number for CHS.
	 * Bit 4: Slave Bit. (0: Selecting Master Drive, 1: Selecting Slave Drive).
	 * Bit 5: Obsolete and isn't used, but should be set.
	 * Bit 6: LBA (0: CHS, 1: LBA).
	 * Bit 7: Obsolete and isn't used, but should be set.
	 * Setting bits 5 and 7 is done by ORing with 0xA0
	 */
	if (lba_mode == 0)
		IDE::write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head); // Drive & CHS.
	else
		IDE::write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head); // Drive & LBA

	// (V) Write Parameters;
	if (lba_mode == 2) {
		IDE::write(channel, ATA_REG_SECCOUNT1,   0);
		IDE::write(channel, ATA_REG_LBA3,   lba_io[3]);
		IDE::write(channel, ATA_REG_LBA4,   lba_io[4]);
		IDE::write(channel, ATA_REG_LBA5,   lba_io[5]);
	}
	IDE::write(channel, ATA_REG_SECCOUNT0,   numsects);
	IDE::write(channel, ATA_REG_LBA0,   lba_io[0]);
	IDE::write(channel, ATA_REG_LBA1,   lba_io[1]);
	IDE::write(channel, ATA_REG_LBA2,   lba_io[2]);

	// (VI) Select the command and send it;
	// Routine that is followed:
	// If ( DMA & LBA48)   DO_DMA_EXT;
	// If ( DMA & LBA28)   DO_DMA_LBA;
	// If ( DMA & LBA28)   DO_DMA_CHS;
	// If (!DMA & LBA48)   DO_PIO_EXT;
	// If (!DMA & LBA28)   DO_PIO_LBA;
	// If (!DMA & !LBA#)   DO_PIO_CHS;
	if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
	if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
	if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;
	if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
	if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
	if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
	if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
	if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
	if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
	if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
	if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
	if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
	IDE::write(channel, ATA_REG_COMMAND, cmd);               // Send the Command.

	if (dma)
		if (direction == 0)
		{
			// DMA Read.
			printf_error("DMA read not implemented");
			return -1;
		}
		else
		{
			// DMA Write.
			printf_error("DMA write not implemented");
			return -1;
		}
	else if (direction == 0)
	{
		// PIO Read.
		for (i = 0; i < numsects; i++)
		{
			if ((err = IDE::polling(channel, 1)))
				return err; // Polling, set error and exit if there is.
			asm("pushw %es");
			asm("mov %%ax, %%es" : : "a"(selector));
			asm("rep insw" : : "c"(words), "d"(bus), "D"(edi)); // Receive Data.
			asm("popw %es");
			edi += (words * 2);
		}
	}
	else
	{
		// PIO Write.
		for (i = 0; i < numsects; i++) {
			IDE::polling(channel, 0); // Polling.
			asm("pushw %ds");
			asm("mov %%ax, %%ds"::"a"(selector));
			asm("rep outsw"::"c"(words), "d"(bus), "S"(edi)); // Send Data
			asm("popw %ds");
			edi += (words*2);
		}
		IDE::write(channel, ATA_REG_COMMAND, (unsigned char []) {   ATA_CMD_CACHE_FLUSH,
														  ATA_CMD_CACHE_FLUSH,
														  ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
		IDE::polling(channel, 0); // Polling.
	}

	return 0; // Easy, isn't it?
}

int ATA::read_sectors(unsigned char drive, unsigned char numsects, unsigned int lba,
					  unsigned short es, unsigned int edi) {

	// 1: Check if the drive presents:
	// ==================================
	if (!ATA::drive_present(drive)) return 0x1;      // Drive Not Found!

		// 2: Check if inputs are valid:
		// ==================================
	else if (((lba + numsects) > IDE::devices[drive].Size) && (IDE::devices[drive].Type == IDE_ATA))
		return 0x2;                     // Seeking to invalid position.

		// 3: Read in PIO Mode through Polling & IRQs:
		// ============================================
	else {
		unsigned char err;
		if (IDE::devices[drive].Type == IDE_ATA)
			err = access(ATA_READ, drive, lba, numsects, es, edi);
		else/* if (IDE::devices[drive].Type == IDE_ATAPI)*/
			err = 4;
		return IDE::print_error(drive, err);
	}
}

int ATA::write_sectors(unsigned char drive, unsigned char numsects, unsigned int lba,
					   unsigned short es, unsigned int edi) {

	// 1: Check if the drive presents:
	// ==================================
	if (!ATA::drive_present(drive))
		return 0x1;      // Drive Not Found!
		// 2: Check if inputs are valid:
		// ==================================
	else if (((lba + numsects) > IDE::devices[drive].Size) && (IDE::devices[drive].Type == IDE_ATA))
		return 0x2;                     // Seeking to invalid position.
		// 3: Write in PIO Mode through Polling & IRQs:
		// ============================================
	else {
		unsigned char err;
		if (IDE::devices[drive].Type == IDE_ATA)
			err = access(ATA_WRITE, drive, lba, numsects, es, edi);
		else /*if (IDE::devices[drive].Type == IDE_ATAPI)*/
			err = 4;
		return IDE::print_error(drive, err);
	}
}

void ATA::init()
{
	IDE::init();
}

unsigned int ATA::get_drive_size(unsigned char drive)
{
	if (drive > 3 || IDE::devices[drive].Reserved == 0)
		return 0;      // Drive Not Found!

	return IDE::devices[drive].Size * ATA_SECTOR_SIZE;
}

bool ATA::drive_present(unsigned char drive)
{
	return !(drive > 3 || IDE::devices[drive].Reserved == 0);
}
