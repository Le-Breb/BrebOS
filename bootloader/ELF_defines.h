#ifndef CUSTOM_OS_ELF_DEFINES_H
#define CUSTOM_OS_ELF_DEFINES_H
//http://www.skyfree.org/linux/references/ELF_Format.pdf
//https://gist.github.com/x0nu11byt3/bcb35c3de461e5fb66173071a2379779

#include "config.h"

#define EI_NIDENT (16)

typedef uint Elf32_Off;
typedef uint Elf32_Word;
typedef signed int Elf32_Sword;
typedef uint Elf32_Addr;
typedef unsigned short Elf32_Half;
typedef unsigned short Elf32_Section;

typedef struct
{
	unsigned char e_ident[EI_NIDENT];    /* Magic number and other info */
	Elf32_Half e_type;            /* Object file type */
	Elf32_Half e_machine;        /* Architecture */
	Elf32_Word e_version;        /* Object file version */
	Elf32_Addr e_entry;        /* Entry point virtual address */
	Elf32_Off e_phoff;        /* Program header table file offset */
	Elf32_Off e_shoff;        /* Section header table file offset */
	Elf32_Word e_flags;        /* Processor-specific flags */
	Elf32_Half e_ehsize;        /* ELF header size in bytes */
	Elf32_Half e_phentsize;        /* Program header table entry size */
	Elf32_Half e_phnum;        /* Program header table entry count */
	Elf32_Half e_shentsize;        /* Section header table entry size */
	Elf32_Half e_shnum;        /* Section header table entry count */
	Elf32_Half e_shstrndx;        /* Section header string table index */
} Elf32_Ehdr;

// e_type

#define ET_NONE        0        /* No file type */
#define ET_REL        1        /* Relocatable file */
#define ET_EXEC        2        /* Executable file */
#define ET_DYN        3        /* Shared object file */
#define ET_CORE        4        /* Core file */
#define    ET_NUM        5        /* Number of defined types */
#define ET_LOOS        0xfe00        /* OS-specific range start */
#define ET_HIOS        0xfeff        /* OS-specific range end */
#define ET_LOPROC    0xff00        /* Processor-specific range start */
#define ET_HIPROC    0xffff        /* Processor-specific range end */

// e_machine

#define EM_NONE         0    /* No machine */
#define EM_M32         1    /* AT&T WE 32100 */
#define EM_SPARC     2    /* SUN SPARC */
#define EM_386         3    /* Intel 80386 */
#define EM_68K         4    /* Motorola m68k family */
#define EM_88K         5    /* Motorola m88k family */
#define EM_IAMCU     6    /* Intel MCU */
#define EM_860         7    /* Intel 80860 */
#define EM_MIPS         8    /* MIPS R3000 big-endian */
#define EM_S370         9    /* IBM System/370 */
#define EM_MIPS_RS3_LE    10    /* MIPS R3000 little-endian */
/* reserved 11-14 */
#define EM_PARISC    15    /* HPPA */
/* reserved 16 */
#define EM_VPP500    17    /* Fujitsu VPP500 */
#define EM_SPARC32PLUS    18    /* Sun's "v8plus" */
#define EM_960        19    /* Intel 80960 */
#define EM_PPC        20    /* PowerPC */
#define EM_PPC64    21    /* PowerPC 64-bit */
#define EM_S390        22    /* IBM S390 */
#define EM_SPU        23    /* IBM SPU/SPC */
/* reserved 24-35 */
#define EM_V800        36    /* NEC V800 series */
#define EM_FR20        37    /* Fujitsu FR20 */
#define EM_RH32        38    /* TRW RH-32 */
#define EM_RCE        39    /* Motorola RCE */
#define EM_ARM        40    /* ARM */
#define EM_FAKE_ALPHA    41    /* Digital Alpha */
#define EM_SH        42    /* Hitachi SH */
#define EM_SPARCV9    43    /* SPARC v9 64-bit */
#define EM_TRICORE    44    /* Siemens Tricore */
#define EM_ARC        45    /* Argonaut RISC Core */
#define EM_H8_300    46    /* Hitachi H8/300 */
#define EM_H8_300H    47    /* Hitachi H8/300H */
#define EM_H8S        48    /* Hitachi H8S */
#define EM_H8_500    49    /* Hitachi H8/500 */
#define EM_IA_64    50    /* Intel Merced */
#define EM_MIPS_X    51    /* Stanford MIPS-X */
#define EM_COLDFIRE    52    /* Motorola Coldfire */
#define EM_68HC12    53    /* Motorola M68HC12 */
#define EM_MMA        54    /* Fujitsu MMA Multimedia Accelerator */
#define EM_PCP        55    /* Siemens PCP */
#define EM_NCPU        56    /* Sony nCPU embeeded RISC */
#define EM_NDR1        57    /* Denso NDR1 microprocessor */
#define EM_STARCORE    58    /* Motorola Start*Core processor */
#define EM_ME16        59    /* Toyota ME16 processor */
#define EM_ST100    60    /* STMicroelectronic ST100 processor */
#define EM_TINYJ    61    /* Advanced Logic Corp. Tinyj emb.fam */
#define EM_X86_64    62    /* AMD x86-64 architecture */
#define EM_PDSP        63    /* Sony DSP Processor */
#define EM_PDP10    64    /* Digital PDP-10 */
#define EM_PDP11    65    /* Digital PDP-11 */
#define EM_FX66        66    /* Siemens FX66 microcontroller */
#define EM_ST9PLUS    67    /* STMicroelectronics ST9+ 8/16 mc */
#define EM_ST7        68    /* STmicroelectronics ST7 8 bit mc */
#define EM_68HC16    69    /* Motorola MC68HC16 microcontroller */
#define EM_68HC11    70    /* Motorola MC68HC11 microcontroller */
#define EM_68HC08    71    /* Motorola MC68HC08 microcontroller */
#define EM_68HC05    72    /* Motorola MC68HC05 microcontroller */
#define EM_SVX        73    /* Silicon Graphics SVx */
#define EM_ST19        74    /* STMicroelectronics ST19 8 bit mc */
#define EM_VAX        75    /* Digital VAX */
#define EM_CRIS        76    /* Axis Communications 32-bit emb.proc */
#define EM_JAVELIN    77    /* Infineon Technologies 32-bit emb.proc */
#define EM_FIREPATH    78    /* Element 14 64-bit DSP Processor */
#define EM_ZSP        79    /* LSI Logic 16-bit DSP Processor */
#define EM_MMIX        80    /* Donald Knuth's educational 64-bit proc */
#define EM_HUANY    81    /* Harvard University machine-independent object files */
#define EM_PRISM    82    /* SiTera Prism */
#define EM_AVR        83    /* Atmel AVR 8-bit microcontroller */
#define EM_FR30        84    /* Fujitsu FR30 */
#define EM_D10V        85    /* Mitsubishi D10V */
#define EM_D30V        86    /* Mitsubishi D30V */
#define EM_V850        87    /* NEC v850 */
#define EM_M32R        88    /* Mitsubishi M32R */
#define EM_MN10300    89    /* Matsushita MN10300 */
#define EM_MN10200    90    /* Matsushita MN10200 */
#define EM_PJ        91    /* picoJava */
#define EM_OPENRISC    92    /* OpenRISC 32-bit embedded processor */
#define EM_ARC_COMPACT    93    /* ARC International ARCompact */
#define EM_XTENSA    94    /* Tensilica Xtensa Architecture */
#define EM_VIDEOCORE    95    /* Alphamosaic VideoCore */
#define EM_TMM_GPP    96    /* Thompson Multimedia General Purpose Proc */
#define EM_NS32K    97    /* National Semi. 32000 */
#define EM_TPC        98    /* Tenor Network TPC */
#define EM_SNP1K    99    /* Trebia SNP 1000 */
#define EM_ST200    100    /* STMicroelectronics ST200 */
#define EM_IP2K        101    /* Ubicom IP2xxx */
#define EM_MAX        102    /* MAX processor */
#define EM_CR        103    /* National Semi. CompactRISC */
#define EM_F2MC16    104    /* Fujitsu F2MC16 */
#define EM_MSP430    105    /* Texas Instruments msp430 */
#define EM_BLACKFIN    106    /* Analog Devices Blackfin DSP */
#define EM_SE_C33    107    /* Seiko Epson S1C33 family */
#define EM_SEP        108    /* Sharp embedded microprocessor */
#define EM_ARCA        109    /* Arca RISC */
#define EM_UNICORE    110    /* PKU-Unity & MPRC Peking Uni. mc series */
#define EM_EXCESS    111    /* eXcess configurable cpu */
#define EM_DXP        112    /* Icera Semi. Deep Execution Processor */
#define EM_ALTERA_NIOS2 113    /* Altera Nios II */
#define EM_CRX        114    /* National Semi. CompactRISC CRX */
#define EM_XGATE    115    /* Motorola XGATE */
#define EM_C166        116    /* Infineon C16x/XC16x */
#define EM_M16C        117    /* Renesas M16C */
#define EM_DSPIC30F    118    /* Microchip Technology dsPIC30F */
#define EM_CE        119    /* Freescale Communication Engine RISC */
#define EM_M32C        120    /* Renesas M32C */
/* reserved 121-130 */
#define EM_TSK3000    131    /* Altium TSK3000 */
#define EM_RS08        132    /* Freescale RS08 */
#define EM_SHARC    133    /* Analog Devices SHARC family */
#define EM_ECOG2    134    /* Cyan Technology eCOG2 */
#define EM_SCORE7    135    /* Sunplus S+core7 RISC */
#define EM_DSP24    136    /* New Japan Radio (NJR) 24-bit DSP */
#define EM_VIDEOCORE3    137    /* Broadcom VideoCore III */
#define EM_LATTICEMICO32 138    /* RISC for Lattice FPGA */
#define EM_SE_C17    139    /* Seiko Epson C17 */
#define EM_TI_C6000    140    /* Texas Instruments TMS320C6000 DSP */
#define EM_TI_C2000    141    /* Texas Instruments TMS320C2000 DSP */
#define EM_TI_C5500    142    /* Texas Instruments TMS320C55x DSP */
#define EM_TI_ARP32    143    /* Texas Instruments App. Specific RISC */
#define EM_TI_PRU    144    /* Texas Instruments Prog. Realtime Unit */
/* reserved 145-159 */
#define EM_MMDSP_PLUS    160    /* STMicroelectronics 64bit VLIW DSP */
#define EM_CYPRESS_M8C    161    /* Cypress M8C */
#define EM_R32C        162    /* Renesas R32C */
#define EM_TRIMEDIA    163    /* NXP Semi. TriMedia */
#define EM_QDSP6    164    /* QUALCOMM DSP6 */
#define EM_8051        165    /* Intel 8051 and variants */
#define EM_STXP7X    166    /* STMicroelectronics STxP7x */
#define EM_NDS32    167    /* Andes Tech. compact code emb. RISC */
#define EM_ECOG1X    168    /* Cyan Technology eCOG1X */
#define EM_MAXQ30    169    /* Dallas Semi. MAXQ30 mc */
#define EM_XIMO16    170    /* New Japan Radio (NJR) 16-bit DSP */
#define EM_MANIK    171    /* M2000 Reconfigurable RISC */
#define EM_CRAYNV2    172    /* Cray NV2 vector architecture */
#define EM_RX        173    /* Renesas RX */
#define EM_METAG    174    /* Imagination Tech. META */
#define EM_MCST_ELBRUS    175    /* MCST Elbrus */
#define EM_ECOG16    176    /* Cyan Technology eCOG16 */
#define EM_CR16        177    /* National Semi. CompactRISC CR16 */
#define EM_ETPU        178    /* Freescale Extended Time Processing Unit */
#define EM_SLE9X    179    /* Infineon Tech. SLE9X */
#define EM_L10M        180    /* Intel L10M */
#define EM_K10M        181    /* Intel K10M */
/* reserved 182 */
#define EM_AARCH64    183    /* ARM AARCH64 */
/* reserved 184 */
#define EM_AVR32    185    /* Amtel 32-bit microprocessor */
#define EM_STM8        186    /* STMicroelectronics STM8 */
#define EM_TILE64    187    /* Tileta TILE64 */
#define EM_TILEPRO    188    /* Tilera TILEPro */
#define EM_MICROBLAZE    189    /* Xilinx MicroBlaze */
#define EM_CUDA        190    /* NVIDIA CUDA */
#define EM_TILEGX    191    /* Tilera TILE-Gx */
#define EM_CLOUDSHIELD    192    /* CloudShield */
#define EM_COREA_1ST    193    /* KIPO-KAIST Core-A 1st gen. */
#define EM_COREA_2ND    194    /* KIPO-KAIST Core-A 2nd gen. */
#define EM_ARC_COMPACT2    195    /* Synopsys ARCompact V2 */
#define EM_OPEN8    196    /* Open8 RISC */
#define EM_RL78        197    /* Renesas RL78 */
#define EM_VIDEOCORE5    198    /* Broadcom VideoCore V */
#define EM_78KOR    199    /* Renesas 78KOR */
#define EM_56800EX    200    /* Freescale 56800EX DSC */
#define EM_BA1        201    /* Beyond BA1 */
#define EM_BA2        202    /* Beyond BA2 */
#define EM_XCORE    203    /* XMOS xCORE */
#define EM_MCHP_PIC    204    /* Microchip 8-bit PIC(r) */
/* reserved 205-209 */
#define EM_KM32        210    /* KM211 KM32 */
#define EM_KMX32    211    /* KM211 KMX32 */
#define EM_EMX16    212    /* KM211 KMX16 */
#define EM_EMX8        213    /* KM211 KMX8 */
#define EM_KVARC    214    /* KM211 KVARC */
#define EM_CDP        215    /* Paneve CDP */
#define EM_COGE        216    /* Cognitive Smart Memory Processor */
#define EM_COOL        217    /* Bluechip CoolEngine */
#define EM_NORC        218    /* Nanoradio Optimized RISC */
#define EM_CSR_KALIMBA    219    /* CSR Kalimba */
#define EM_Z80        220    /* Zilog Z80 */
#define EM_VISIUM    221    /* Controls and Data Services VISIUMcore */
#define EM_FT32        222    /* FTDI Chip FT32 */
#define EM_MOXIE    223    /* Moxie processor */
#define EM_AMDGPU    224    /* AMD GPU */
/* reserved 225-242 */
#define EM_RISCV    243    /* RISC-V */

#define EM_BPF        247    /* Linux BPF -- in-kernel virtual machine */
#define EM_CSKY        252     /* C-SKY */

#define EM_NUM        253

/* Old spellings/synonyms.  */

#define EM_ARC_A5    EM_ARC_COMPACT

/* If it is necessary to assign new unofficial EM_* values, please
   pick large random numbers (0x8523, 0xa7f2, etc.) to minimize the
   chances of collision with official or non-GNU unofficial values.  */

#define EM_ALPHA    0x9026

// e_version

#define EV_NONE        0        /* Invalid ELF version */
#define EV_CURRENT    1        /* Current version */
#define EV_NUM        2

typedef struct
{
	Elf32_Word sh_name;        /* Section name (string tbl index) */
	Elf32_Word sh_type;        /* Section type */
	Elf32_Word sh_flags;        /* Section flags */
	Elf32_Addr sh_addr;        /* Section virtual addr at execution */
	Elf32_Off sh_offset;        /* Section file offset */
	Elf32_Word sh_size;        /* Section size in bytes */
	Elf32_Word sh_link;        /* Link to another section */
	Elf32_Word sh_info;        /* Additional section information */
	Elf32_Word sh_addralign;        /* Section alignment */
	Elf32_Word sh_entsize;        /* Entry size if section holds table */
} Elf32_Shdr;

// Sh types

#define SHT_NULL      0        /* Section header table entry unused */
#define SHT_PROGBITS      1        /* Program data */
#define SHT_SYMTAB      2        /* Symbol table */
#define SHT_STRTAB      3        /* String table */
#define SHT_RELA      4        /* Relocation entries with addends */
#define SHT_HASH      5        /* Symbol hash table */
#define SHT_DYNAMIC      6        /* Dynamic linking information */
#define SHT_NOTE      7        /* Notes */
#define SHT_NOBITS      8        /* Program space with no data (bss) */
#define SHT_REL          9        /* Relocation entries, no addends */
#define SHT_SHLIB      10        /* Reserved */
#define SHT_DYNSYM      11        /* Dynamic linker symbol table */
#define SHT_INIT_ARRAY      14        /* Array of constructors */
#define SHT_FINI_ARRAY      15        /* Array of destructors */
#define SHT_PREINIT_ARRAY 16        /* Array of pre-constructors */
#define SHT_GROUP      17        /* Section group */
#define SHT_SYMTAB_SHNDX  18        /* Extended section indeces */
#define    SHT_NUM          19        /* Number of defined types.  */
#define SHT_LOOS      0x60000000    /* Start OS-specific.  */
#define SHT_GNU_ATTRIBUTES 0x6ffffff5    /* Object attributes.  */
#define SHT_GNU_HASH      0x6ffffff6    /* GNU-style hash table.  */
#define SHT_GNU_LIBLIST      0x6ffffff7    /* Prelink library list */
#define SHT_CHECKSUM      0x6ffffff8    /* Checksum for DSO content.  */
#define SHT_LOSUNW      0x6ffffffa    /* Sun-specific low bound.  */
#define SHT_SUNW_move      0x6ffffffa
#define SHT_SUNW_COMDAT   0x6ffffffb
#define SHT_SUNW_syminfo  0x6ffffffc
#define SHT_GNU_verdef      0x6ffffffd    /* Version definition section.  */
#define SHT_GNU_verneed      0x6ffffffe    /* Version needs section.  */
#define SHT_GNU_versym      0x6fffffff    /* Version symbol table.  */
#define SHT_HISUNW      0x6fffffff    /* Sun-specific high bound.  */
#define SHT_HIOS      0x6fffffff    /* End OS-specific type */
#define SHT_LOPROC      0x70000000    /* Start of processor-specific */
#define SHT_HIPROC      0x7fffffff    /* End of processor-specific */
#define SHT_LOUSER      0x80000000    /* Start of application-specific */
#define SHT_HIUSER      0x8fffffff    /* End of application-specific */

// Sh flags

#define SHF_WRITE         (1 << 0)    /* Writable */
#define SHF_ALLOC         (1 << 1)    /* Occupies memory during execution */
#define SHF_EXECINSTR         (1 << 2)    /* Executable */
#define SHF_MERGE         (1 << 4)    /* Might be merged */
#define SHF_STRINGS         (1 << 5)    /* Contains nul-terminated strings */
#define SHF_INFO_LINK         (1 << 6)    /* `sh_info' contains SHT index */
#define SHF_LINK_ORDER         (1 << 7)    /* Preserve order after combining */
#define SHF_OS_NONCONFORMING (1 << 8)    /* Non-standard OS specific handling
					   required */
#define SHF_GROUP         (1 << 9)    /* Section is member of a group.  */
#define SHF_TLS             (1 << 10)    /* Section hold thread-local data.  */
#define SHF_COMPRESSED         (1 << 11)    /* Section with compressed data. */
#define SHF_MASKOS         0x0ff00000    /* OS-specific.  */
#define SHF_MASKPROC         0xf0000000    /* Processor-specific */
#define SHF_ORDERED         (1 << 30)    /* Special ordering requirement
					   (Solaris).  */
#define SHF_EXCLUDE         (1U << 31)    /* Section is excluded unless
					   referenced or allocated (Solaris).*/

typedef struct
{
	Elf32_Word p_type;            /* Segment type */
	Elf32_Off p_offset;        /* Segment file offset */
	Elf32_Addr p_vaddr;        /* Segment virtual address */
	Elf32_Addr p_paddr;        /* Segment physical address */
	Elf32_Word p_filesz;        /* Segment size in file */
	Elf32_Word p_memsz;        /* Segment size in memory */
	Elf32_Word p_flags;        /* Segment flags */
	Elf32_Word p_align;        /* Segment alignment */
} Elf32_Phdr;

// p_type

#define    PT_NULL        0        /* Program header table entry unused */
#define PT_LOAD        1        /* Loadable program segment */
#define PT_DYNAMIC    2        /* Dynamic linking information */
#define PT_INTERP    3        /* Program interpreter */
#define PT_NOTE        4        /* Auxiliary information */
#define PT_SHLIB    5        /* Reserved */
#define PT_PHDR        6        /* Entry for header table itself */
#define PT_TLS        7        /* Thread-local storage segment */
#define    PT_NUM        8        /* Number of defined types */
#define PT_LOOS        0x60000000    /* Start of OS-specific */
#define PT_GNU_EH_FRAME    0x6474e550    /* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK    0x6474e551    /* Indicates stack executability */
#define PT_GNU_RELRO    0x6474e552    /* Read-only after relocation */
#define PT_LOSUNW    0x6ffffffa
#define PT_SUNWBSS    0x6ffffffa    /* Sun Specific segment */
#define PT_SUNWSTACK    0x6ffffffb    /* Stack segment */
#define PT_HISUNW    0x6fffffff
#define PT_HIOS        0x6fffffff    /* End of OS-specific */
#define PT_LOPROC    0x70000000    /* Start of processor-specific */
#define PT_HIPROC    0x7fffffff    /* End of processor-specific */

// p_flags

#define PF_X        (1 << 0)    /* Segment is executable */
#define PF_W        (1 << 1)    /* Segment is writable */
#define PF_R        (1 << 2)    /* Segment is readable */
#define PF_MASKOS    0x0ff00000    /* OS-specific */
#define PF_MASKPROC    0xf0000000    /* Processor-specific */

typedef struct
{
	Elf32_Word st_name;        /* Symbol name (string tbl index) */
	Elf32_Addr st_value;        /* Symbol value */
	Elf32_Word st_size;        /* Symbol size */
	unsigned char st_info;        /* Symbol type and binding */
	unsigned char st_other;        /* Symbol visibility */
	Elf32_Section st_shndx;        /* Section index */
} Elf32_Sym;

// st_info

#define ELF32_ST_BIND(val)        (((unsigned char) (val)) >> 4)
#define ELF32_ST_TYPE(val)        ((val) & 0xf)
#define ELF32_ST_INFO(bind, type)    (((bind) << 4) + ((type) & 0xf))

// st_bind
#define STB_LOCAL    0        /* Local symbol */
#define STB_GLOBAL    1        /* Global symbol */
#define STB_WEAK    2        /* Weak symbol */
#define    STB_NUM        3        /* Number of defined types.  */
#define STB_LOOS    10        /* Start of OS-specific */
#define STB_GNU_UNIQUE    10        /* Unique symbol.  */
#define STB_HIOS    12        /* End of OS-specific */
#define STB_LOPROC    13        /* Start of processor-specific */
#define STB_HIPROC    15        /* End of processor-specific */

// st_type

#define STT_NOTYPE    0        /* Symbol type is unspecified */
#define STT_OBJECT    1        /* Symbol is a data object */
#define STT_FUNC    2        /* Symbol is a code object */
#define STT_SECTION    3        /* Symbol associated with a section */
#define STT_FILE    4        /* Symbol's name is file name */
#define STT_COMMON    5        /* Symbol is a common data object */
#define STT_TLS        6        /* Symbol is thread-local data object*/
#define    STT_NUM        7        /* Number of defined types.  */
#define STT_LOOS    10        /* Start of OS-specific */
#define STT_GNU_IFUNC    10        /* Symbol is indirect code object */
#define STT_HIOS    12        /* End of OS-specific */
#define STT_LOPROC    13        /* Start of processor-specific */
#define STT_HIPROC    15        /* End of processor-specific */

typedef struct
{
	Elf32_Sword d_tag;            /* Dynamic entry type */
	union
	{
		Elf32_Word d_val;            /* Integer value */
		Elf32_Addr d_ptr;            /* Address value */
	} d_un;
} Elf32_Dyn;

// d_tag

#define DT_NULL        0        /* Marks end of dynamic section */
#define DT_NEEDED    1        /* Name of needed library */
#define DT_PLTRELSZ    2        /* Size in bytes of PLT relocs */
#define DT_PLTGOT    3        /* Processor defined value */
#define DT_HASH        4        /* Address of symbol hash table */
#define DT_STRTAB    5        /* Address of string table */
#define DT_SYMTAB    6        /* Address of symbol table */
#define DT_RELA        7        /* Address of Rela relocs */
#define DT_RELASZ    8        /* Total size of Rela relocs */
#define DT_RELAENT    9        /* Size of one Rela reloc */
#define DT_STRSZ    10        /* Size of string table */
#define DT_SYMENT    11        /* Size of one symbol table entry */
#define DT_INIT        12        /* Address of init function */
#define DT_FINI        13        /* Address of termination function */
#define DT_SONAME    14        /* Name of shared object */
#define DT_RPATH    15        /* Library search path (deprecated) */
#define DT_SYMBOLIC    16        /* Start symbol search here */
#define DT_REL        17        /* Address of Rel relocs */
#define DT_RELSZ    18        /* Total size of Rel relocs */
#define DT_RELENT    19        /* Size of one Rel reloc */
#define DT_PLTREL    20        /* Type of reloc in PLT */
#define DT_DEBUG    21        /* For debugging; unspecified */
#define DT_TEXTREL    22        /* Reloc might modify .text */
#define DT_JMPREL    23        /* Address of PLT relocs */
#define DT_BIND_NOW    24        /* Process relocations of object */
#define DT_INIT_ARRAY    25        /* Array with addresses of init fct */
#define DT_FINI_ARRAY    26        /* Array with addresses of fini fct */
#define DT_INIT_ARRAYSZ    27        /* Size in bytes of DT_INIT_ARRAY */
#define DT_FINI_ARRAYSZ    28        /* Size in bytes of DT_FINI_ARRAY */
#define DT_RUNPATH    29        /* Library search path */
#define DT_FLAGS    30        /* Flags for the object being loaded */
#define DT_ENCODING    32        /* Start of encoded range */
#define DT_PREINIT_ARRAY 32        /* Array with addresses of preinit fct*/
#define DT_PREINIT_ARRAYSZ 33        /* size in bytes of DT_PREINIT_ARRAY */
#define DT_SYMTAB_SHNDX    34        /* Address of SYMTAB_SHNDX section */
#define DT_NUM        35        /* Number used */
#define DT_LOOS        0x6000000d    /* Start of OS-specific */
#define DT_HIOS        0x6ffff000    /* End of OS-specific */
#define DT_GNU_HASH  0x6ffffef5       /* Address of GNU symbol hash table (guessed, not from official doc)*/
#define DT_LOPROC    0x70000000    /* Start of processor-specific */
#define DT_HIPROC    0x7fffffff    /* End of processor-specific */
#define DT_PROCNUM    DT_MIPS_NUM    /* Most used by any processor */

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s)<<8)+(unsigned char)t)

typedef struct
{
	Elf32_Addr r_offset;        /* Address */
	Elf32_Word r_info;            /* Relocation type and symbol index */
} Elf32_Rel;

typedef struct
{
	Elf32_Addr r_offset;        /* Address */
	Elf32_Word r_info;            /* Relocation type and symbol index */
	Elf32_Sword r_addend;        /* Addend */
} Elf32_Rela;

#define R_386_NONE 0
#define R_386_32 1
#define R_386_PC32 2
#define R_386_GOT32 3
#define R_386_PLT32 4
#define R_386_COPY 5
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8
#define R_386_GOTOFF 9
#define R_386_GOTPC 100

#define STN_UNDEF 0
#define ELF32_ADDR_ERR ((Elf32_Addr)-1)
#endif //CUSTOM_OS_ELF_DEFINES_H
