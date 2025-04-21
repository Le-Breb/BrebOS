#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <kstddef.h>

#include "ELF.h"
#include "ELF_defines.h"
#include "process.h"
#include "scheduler.h"

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

class ELFLoader
{
    friend class Process;
private:
    Process* current_process;
    init_fini_info init_fini;
    list<elf_dependence_list>* elf_dep_list;
    uint* pte = nullptr;
    uint num_pages = 0;
    Memory::page_table_t* page_tables;
    Memory::pdt_t* pdt;
    uint* sys_page_tables_correspondence;
    stack_state_t stack_state{};
    bool used = false;

    ELFLoader();
    ~ELFLoader();

    /**
    * Sets up a dynamically linked process
    * @param elf process' main ELF
    * @return process set up, NULL if an error occurred
    */
    bool dynamic_loading(ELF* elf);

    /*/**
    * Load a lib in a process' address soace
    * @param path GRUB modules
    * @param lib_dynlk_runtime_entry_point
    * @param runtime_load_address address where lib is loaded in runtime address space
    * @return libdynlk runtime entry point address
    #1#
    ELF* load_lib(const char* path, void* lib_dynlk_runtime_entry_point, Elf32_Addr& runtime_load_address);*/

    /**
     * Allocate space for libydnlk and add it to a process' address space
     * @param elf library elf
     * @return boolean indicating success state
     */
    bool alloc_elf_memory(ELF& elf);

    /**
     * Load part of an ELF segment into the process address space and maps it.
     *
     * @param bytes_ptr pointer to bytes to copy
     * @param n num bytes to copy
     * @param h PT_LOAD segment header
     * @param lib_runtime_load_address runtime load address of the library
     * @param copied_bytes counter of bytes processed in current segment
     */
    void
    copy_elf_subsegment_to_address_space(const void* bytes_ptr, uint n, Elf32_Phdr* h, uint lib_runtime_load_address,
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
     * @param path ELF to load
     * @param expected_type
     * @param lib_dynlk_runtime_entry_point
     * @return runtime load address of the ELF
     */
    bool load_elf(const char* path, ELF_type expected_type, void* lib_dynlk_runtime_entry_point);

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
     * @param runtime_load_address runtime load address of the lib
     */
    void map_elf(const ELF* load_elf, Elf32_Addr runtime_load_address) const;

    /**
     * Create a process to run an ELF executable
     *
     * @return program's process
     */
    Process* build_process(int argc, const char** argv, pid_t pid, pid_t ppid, const char* path, uint);

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
     */
    Elf32_Addr finalize_process_setup(int argc, const char** argv);

    static const char** add_argv0_to_argv(const char** argv, const char* path, int& argc);

    /**
     * Converts an address in runtime address space to an address in current address space
     * @param runtime_address address in runtime address space
     * @return corresponding load address
     */
    [[nodiscard]] Elf32_Addr runtime_address_to_load_address(Elf32_Addr runtime_address) const;

    /**
     * Offsets the memory mapping of the loaded ELF by offset pages to the left
     * @param offset mapping offset
     */
    void offset_memory_mapping(uint offset);
public:
    /**
     * Create a process from an ELF loaded in memory
     * @param pid process ID
     * @param ppid parent process ID
     * @param argc num args
     * @param argv args
     * @param path process main ELF' path
     * @param priority process priority
     * @return process, nullptr if an error occurred
     */
    static Process* setup_elf_process(pid_t pid, pid_t ppid, int argc, const char** argv,
                                      const char* path, uint priority);
};


#endif //ELFLOADER_H
