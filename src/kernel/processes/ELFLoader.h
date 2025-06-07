#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <kstddef.h>

#include "ELF.h"
#include "ELF_defines.h"
#include "process.h"
#include "scheduler.h"
#include "../file_management/dentry.h"

#define LIBDYNLK_PATH "/bin/libdynlk.so"
#define LIBK_PATH "/bin/libk.so"

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
    list<elf_dependence>* elf_dep_list;
    uint num_pages = 0;
    Memory::page_table_t* page_tables;
    Memory::pdt_t* pdt;
    stack_state_t stack_state{};
    bool used = false;
    Elf32_Addr libdynlk_runtime_entry_point = ELF32_ADDR_ERR;

    ELFLoader();
    ~ELFLoader();

    /**
    * Sets up a dynamically linked process
    * @param elf process' main ELF
    * @return process set up, NULL if an error occurred
    */
    bool dynamic_loading(const ELF* elf);

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
    bool alloc_elf_memory(const ELF& elf);

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
    void register_elf_init_and_fini(const ELF* elf, uint runtime_load_address);

    /**
     * Load ELF file code and data into a process' address space and maps it
     * @param file ELF to load
     * @param expected_type
     * @return Loaded ELF, nullptr on error
     */
    ELF* load_elf(const SharedPointer<Dentry>& file, ELF_type expected_type);

    ELF* load_elf(void* buf, ELF_type expected_type);

    ELF* load_libdynlk();

    /**
     * Process relocations of an ELF.
     * @param elf elf with relocations to process
     * @param elf_runtime_load_address where the elf is loaded at runtime
     */
    void apply_relocations(ELF* elf, uint elf_runtime_load_address) const;

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
    Process* build_process(int argc, const char** argv, pid_t pid, pid_t ppid, const SharedPointer<Dentry>& file, uint);

    /**
     * Writes argc, argv array pointer, argv pointer array and argv contents to stack
     * @param stack_top_v_addr Virtual address of process stack top in kernel address space
     * @param argc number of arguments
     * @param argv argument list
     * @param init_array init array. Shall contain _init at index 0 if it exists
     * @param fini_array fini array. Shall contain _fini at index 0 if it exists
     * @return ESP in process address space ready to be used
     */
    static size_t write_args_to_stack(size_t stack_top_v_addr, int argc, const char** argv, const list<Elf32_Addr>
                                      & init_array, const list<Elf32_Addr>& fini_array);

    /**
     * Process setup last phase: once the main ELF has been loaded, this function executes the remaining setup actions:
     * setup process PDT, page tables, pid, ppid, segment selectors, priority, registers and stack
     * @param argc num args
     * @param argv args
     */
    Elf32_Addr finalize_process_setup(int argc, const char** argv);

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
    void offset_memory_mapping(uint offset) const;

    void load_elf_code(const ELF* elf, uint load_address, uint runtime_load_address) const;

    void setup_elf_got(const ELF* elf, uint elf_runtime_load_address) const;

    Elf32_Addr get_libdynlk_runtime_address();

    void setup_stacks(Elf32_Addr& k_stack_top, Elf32_Addr& p_stack_top_v_addr) const;

    void setup_pcb(uint p_stack_top_v_addr, int argc, const char** argv);

    void unmap_new_process_from_current_process() const;

    void setup_pdt();
public:
    /**
     * Create a process from an ELF loaded in memory
     * @param pid process ID
     * @param ppid parent process ID
     * @param argc num args
     * @param argv args
     * @param file process main ELF' path
     * @param priority process priority
     * @return process, nullptr if an error occurred
     */
    static Process* setup_elf_process(pid_t pid, pid_t ppid, int argc, const char** argv,
                                      const SharedPointer<Dentry>& file, uint priority);
};


#endif //ELFLOADER_H
