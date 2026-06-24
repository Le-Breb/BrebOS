#include "GDB.h"
#include "../core/memory.h"
#include "abi-bits/vm-flags.h"
#include <stdint.h>

#include "../processes/process.h"
#include "../processes/scheduler.h"
#include "abi-bits/fcntl.h"

const char GDB::host_path_prefix[] = "src/programs/build/";
const char GDB::vm_path_prefix[] = "/bin/";
const void* GDB::gdb_struct_addr = (void*)0xD0000000;
GDB::process_info* GDB::gdb_struct = nullptr;

// gdb_elf_loader.py sets silent breakpoints on those functions. This is
// how the vm is able to send an event to the host
extern "C"
__attribute__((noinline))
void __gdb_load_process() // NOLINT(*-reserved-identifier)
{
    asm volatile("" ::: "memory");
}

extern "C"
__attribute__((noinline))
void __gdb_unload_process() // NOLINT(*-reserved-identifier)
{
    asm volatile("" ::: "memory");
}


// This could be done on host with readelf and grep at build time. It'd be faster, but doing it from here is way more
// stylish :D
void* GDB::find_elf_text_base_addr(const char* elf_name)
{
#define ret_err(msg, ...) {free(abs_path); printf_warn(msg, __VA_ARGS__); return nullptr;}
    auto abs_path = add_prefix_to_elf_name(elf_name, vm_path_prefix);
    auto p = Memory::kernel_process;

    const int fd = p->open(abs_path, O_RDONLY, 0777);
    if (fd < 0)
        ret_err("%s: couldn't open ELF '%s'", __PRETTY_FUNCTION__, elf_name)

    // Get global headers
    Elf32_Ehdr ehdr;
    if (0 > p->read(fd, &ehdr, sizeof(Elf32_Ehdr)))
        ret_err("%s: couldn't read ELF header", __PRETTY_FUNCTION__);

    // Following errors aren't unrecoverable, but im too lazzy to properly free buffers before returning
    // Get sections
    auto* shdrs = (Elf32_Shdr*)malloc(ehdr.e_shnum * ehdr.e_shentsize);
    if (0 > p->lseek(fd, ehdr.e_shoff, SEEK_SET))
        irrecoverable_error("%s: couldn't lseek", __PRETTY_FUNCTION__);
    if (0 > p->read(fd, shdrs, sizeof(Elf32_Shdr) * ehdr.e_shnum))
        irrecoverable_error("%s: couldn't read ELF SHDRs", __PRETTY_FUNCTION__);

    // Get shstrtab
    const auto& sh_strtab_hdr = shdrs[ehdr.e_shstrndx];
    char* shstrtab = new char[sh_strtab_hdr.sh_size];
    if (0 > p->lseek(fd, sh_strtab_hdr.sh_offset, SEEK_SET))
        irrecoverable_error("%s: couldn't lseek", __PRETTY_FUNCTION__);
    if (0 > p->read(fd, shstrtab, sh_strtab_hdr.sh_size))
        irrecoverable_error("%s: couldn't shstrtab", __PRETTY_FUNCTION__);

    // Find .text section
    Elf32_Shdr* text_shdr = nullptr;
    for (auto i = 1; i < ehdr.e_shnum; i++)
        if (!strcmp(&shstrtab[shdrs[i].sh_name], ".text"))
        {
            text_shdr = shdrs + i;
            break;
        }

    if (!text_shdr)
        irrecoverable_error("%s: couldn't find .text", __PRETTY_FUNCTION__);

    p->close(fd);
    free(abs_path);
    free(shdrs);
    delete[] shstrtab;

    return (void*) text_shdr->sh_addr;
}

char* GDB::add_prefix_to_elf_name(const char* path, const char* prefix)
{
    const auto len = strlen(prefix) + strlen(path) + 1;
    auto p = (char*)malloc(len);
    p[0] = '\0';
    p = strcat(p, prefix);
    p = strcat(p, path);

    return p;
}

[[maybe_unused]] // This function requires -debugcon qemu flag to work properly
static void debugcon_putc(char c)
{
    asm volatile ("outb %0, %1"
                  :
                  : "a"(c), "Nd"((uint16_t)0xE9));
}

[[maybe_unused]]
static void debugcon_pustring(const char* string)
{
    for (; *string; debugcon_putc(*string++)) {};
}

GDB::GDB()
{
    int err;
    static_assert(PAGE_SIZE >= sizeof(process_info));
    gdb_struct = (process_info*)Memory::mmap((void*)gdb_struct_addr, PAGE_SIZE,
                                             PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, 0, 0, err, Memory::kernel_process, false, false);
    // const auto page_id = ADDR_PAGE((uint)gdb_struct_addr);
    // Scheduler::get_running_process()->update_pte(page_id, PTE(Memory::page_tables, page_id), true);
    if (!gdb_struct)
        irrecoverable_error("%s: couldn't allocate gdb_struct", __PRETTY_FUNCTION__);
}

GDB* GDB::get_instance()
{
    static GDB* instance = nullptr;

    return instance ? instance : (instance = new GDB());
}

bool GDB::is_elf_loaded(const char* elf_name) const
{
    const auto abs_path = add_prefix_to_elf_name(elf_name, host_path_prefix);
    bool found = false;
    for (const auto& [path, _] : loaded_processes)
    {
        if (!strcmp(path, abs_path))
        {
            found = true;
            break;
        }
    }

    free(abs_path);
    return found;
}

void GDB::load_elf(const char* elf_name)
{
    if (is_elf_loaded(elf_name))
        return;

    void* ba = find_elf_text_base_addr(elf_name);
    if (!ba)
        return;

    auto abs_path = add_prefix_to_elf_name(elf_name, host_path_prefix);
    const process_info pi{strdup(abs_path), (uint)ba};
    loaded_processes.add(pi);
    *gdb_struct = pi;
    free(abs_path);

    __gdb_load_process(); // Trigger python breakpoint handler
}

void GDB::unload_elf(const char* elf_name)
{
    const auto abs_path = add_prefix_to_elf_name(elf_name, host_path_prefix);
    for (const auto& pi : loaded_processes)
    {
        if (!strcmp(pi.path, abs_path))
        {
            delete pi.path;
            free(abs_path);
            *gdb_struct = pi;
            loaded_processes.remove(pi);
            __gdb_unload_process(); // Trigger python breakpoint handler
            return;
        }
    }

    free(abs_path);
    printf_warn("%s: couldn't find ELF '%s'", __PRETTY_FUNCTION__, elf_name);
}
