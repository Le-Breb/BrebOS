#include "ATA.h"
#include "ELFLoader.h"

void main()
{    // Init
    ATA::init();

    // Load
    uint remaining_sectors = N_SECTORS;
    uint sec = START_READ_SECTOR;
    uint load_addr = ELF_LOAD_ADDRESS;
    while (remaining_sectors > 255)
    {
        ATA::read_sectors(DRIVE, 255, sec, DS, load_addr);
        sec += 255;
        load_addr += SECTOR_SIZE * 255;
        remaining_sectors -= 255;
    }
    ATA::read_sectors(DRIVE, remaining_sectors, sec, DS, load_addr);

    ELFLoader{}.load_elf((void*)ELF_LOAD_ADDRESS, Executable);

    // Jump
    ((void (*)())ELF_LOAD_ADDRESS)();
}
