#ifndef UTILS_H
#define UTILS_H
typedef unsigned int uint;
typedef unsigned long size_t;

#define KERNEL_VIRTUAL_BASE 0xC0000000
#define PAGE_SIZE 4096
#define N_SECTORS 1400
#define START_READ_SECTOR 22
#define ELF_LOAD_ADDRESS 0x100000
#define DRIVE 0
#define SECTOR_SIZE 512
#define DS 0x10
#endif