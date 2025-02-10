#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <kstddef.h>

#include "ELF.h"
#include "ELF_defines.h"
#include "process.h"

#define LIBDYNLK_PATH "/bin/libdynlk.so"
#define LIBC_PATH "/bin/libc.so"

struct init_fini_info
{
    list<Elf32_Addr> init_array;
    list<Elf32_Addr> fini_array;

    init_fini_info()
    {
        init_array = list<Elf32_Addr>();
        fini_array = list<Elf32_Addr>();
    }
};

class ELFLoader {
private:
    Process* proc;
    init_fini_info init_fini;

    ELFLoader() : proc(nullptr)
    {
        init_fini = init_fini_info();
    }

    /**
    * Sets up a dynamically linked process
    * @param start_address ELF start address
    * @param elf process' main ELF
    * @return process set up, NULL if an error occurred
    */
    bool dynamic_loading(uint start_address, ELF* elf);

    /**
    * Load a lib in a process' address soace
    * @param path GRUB modules
    * @param lib_dynlk_runtime_entry_point
    * @param runtime_load_address address where lib is loaded in runtime address space
    * @return libdynlk runtime entry point address
    */
    ELF* load_lib(const char* path, void* lib_dynlk_runtime_entry_point, Elf32_Addr& runtime_load_address);

    /**
     * Allocate space for libydnlk and add it to a process' address space
     * @param lib_elf library elf
     * @return boolean indicating success state
     */
    bool alloc_and_add_lib_pages_to_process(ELF& lib_elf) const;

    /**
     * Allocate a process containing an ELF
     * @return process struct, NULL if an error occurred
     */
    static Process* init_process(ELF* elf, const char* path);

    /**
     * Load part of an ELF segment into the process address space and maps it.
     *
     * @param bytes_ptr pointer to bytes to copy
     * @param n num bytes to copy
     * @param h PT_LOAD segment header
     * @param pte_offset offset to consider when referring to process pte, ie index of ELF first PTE entry
     * @param copied_bytes counter of bytes processed in current segment
     */
    void
    copy_elf_subsegment_to_address_space(void* bytes_ptr, uint n, Elf32_Phdr* h, uint& pte_offset,
                                         uint& copied_bytes) const;

    /**
     * Collects _init, _fini addresses, as well as init_array and fini_array entries
     *
     * @param elf ELF to process
     * @param runtime_load_address load address of the ELF at runtime
     */
    void register_elf_init_and_fini(ELF* elf, uint runtime_load_address);

    /**
     * Load ELF file code and data into a process' address space and maps it
     * @param load_elf ELF to load
     * @param elf_start_address start address of ELF
     */
    Elf32_Addr load_elf(ELF* load_elf, uint elf_start_address);

    /**
     * Process relocations of an ELF.
     * @param elf elf with relocations to process
     * @param elf_runtime_load_address where the elf is loaded at runtime
     */
    bool apply_relocations(ELF* elf, uint elf_runtime_load_address) const;

    /**
     * Maps the segments of an ELF into the process' virtual address space
     *
     * @param load_elf ELF to map
     * @param pte_offset number of pages already occupied in process' virtual address space
     */
    void map_elf(ELF* load_elf, uint pte_offset) const;

    /**
     * Create a process to run an ELF executable
     *
     * @return program's process
     */
    Process* process_from_elf(uint start_address, int argc, const char** argv, pid_t pid, pid_t ppid, const char* path);

    /**
     * Writes argc, argv array pointer, argv pointer array and argv contents to stack
     * @param stack_top_v_addr Virtual address of process stack top in kernel address space
     * @param argc number of arguments
     * @param argv argument list
     * @param init_array init array. Shall contain _init at index 0 if it exists
     * @param fini_array fini array. Shall contain _fini at index 0 if it exists
     * @return ESP in process address space ready to be used
     */
    static size_t write_args_to_stack(size_t stack_top_v_addr, int argc, const char** argv, list<Elf32_Addr>
                                      init_array, list<Elf32_Addr> fini_array);

    /**
     * Process setup last phase: once the main ELF has been loaded, this function executes the remaining setup actions:
     * setup process PDT, page tables, pid, ppid, segment selectors, priority, registers and stack
     * @param argc num args
     * @param argv args
     * @param pid process ID
     * @param ppid parent process ID
     */
    void finalize_process_setup(int argc, const char** argv, pid_t pid, pid_t ppid) const;

public:
    /**
     * Create a process from an ELF loaded in memory
     * @param start_addresss program's start address
     * @param pid process ID
     * @param ppid parent process ID
     * @param argc num args
     * @param argv args
     * @return process, nullptr if an error occurred
     */
    static Process* setup_elf_process(uint start_addresss, pid_t pid, pid_t ppid, int argc, const char** argv, const char* path);
};



#endif //ELFLOADER_H
