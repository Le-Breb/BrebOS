#include <stdint.h>

#include "../utils/vector.h"

// cf. https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html

#ifndef MULTIBOOT_HEADER
#define MULTIBOOT_HEADER

#define MULTIBOOT_TAG_TYPE_MMAP              6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER       8

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2

struct multiboot_tag
{
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_framebuffer
{
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
};

struct multiboot_tag_framebuffer_rgb {
    uint8_t framebuffer_red_field_position;
    uint8_t framebuffer_red_mask_size;
    uint8_t framebuffer_green_field_position;
    uint8_t framebuffer_green_mask_size;
    uint8_t framebuffer_blue_field_position;
    uint8_t framebuffer_blue_mask_size;
};

/*
 * ‘mem_lower’ and ‘mem_upper’ indicate the amount of lower and upper memory, respectively, in kilobytes.
 * Lower memory starts at address 0, and upper memory starts at address 1 megabyte.
 * The maximum possible value for lower memory is 640 kilobytes.
 * The value returned for upper memory is maximally the address of the first upper memory hole minus 1 megabyte.
 * It is not guaranteed to be this value.
 */
struct multiboot_tag_basic_mem_info
{
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

struct multiboot_mmap_entry
{
    uint64_t addr;
    uint64_t len;
#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5
    uint32_t type;
    uint32_t zero;
};
typedef struct multiboot_mmap_entry multiboot_memory_map_t;

struct multiboot_tag_mmap
{
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[0];
};

typedef struct multiboot_tag_mmap multiboot_tag_mmap_t;

struct multiboot_info
{
    uint32_t total_size; // total size of the structure including all tags
    uint32_t reserved; // must be zero
    struct multiboot_tag tags[]; // variable-length tag list
};

typedef struct multiboot_info multiboot_info_t;

class Multiboot
{
    static const multiboot_info_t* multiboot_info;
public:
    static void init(const multiboot_info_t* multiboot_info);

    static void* get_tag(uint32_t type);

    static bool is_used;

    static void print_mmap();

    [[nodiscard]]
    static vector<multiboot_memory_map_t> get_mmap();
};

#endif