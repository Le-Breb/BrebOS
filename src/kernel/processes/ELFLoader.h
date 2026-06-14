#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <kstddef.h>

#include "ELF.h"
#include "ELFTools.h"
#include "ELF_defines.h"
#include "process.h"
#include "../file_management/dentry.h"

#define LIBDYNLK_PATH "/bin/libdynlk.so"
#define LIBK_PATH "/bin/libk.so"

#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_NOTELF 10
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_PLATFORM 15
#define AT_HWCAP 16
#define AT_CLKTCK 17
#define AT_SECURE 23
#define AT_BASE_PLATFORM 24
#define AT_RANDOM 25
#define AT_HWCAP2 26
#define AT_EXECFN 31

#define PROCESS_N_STACKS_PAGES (PROCESS_STACK_N_PAGES + PROCESS_SYSCALL_STACK_N_PAGES)

typedef struct auxv_t
{
    int a_type;

    union
    {
        long a_val;
        void* a_ptr;
        void (*a_fnc)();
    };

    auxv_t(int type, long val)
        : a_type(type), a_val(val) {}

    auxv_t(int type, void* ptr)
        : a_type(type), a_ptr(ptr) {}

    auxv_t(int type, void (*fn)())
        : a_type(type), a_fnc(fn) {}

} auxv_t;

class ELFLoader
{
    friend class Process;
private:
    Process* current_process;
    list<elf_dependence>* elf_dep_list;
    uint num_pages = 0;
    Memory::page_table_t* page_tables;
    Memory::pdt_t* pdt;
    stack_state_t stack_state{};
    bool used = false;
    Elf32_Addr libdynlk_runtime_entry_point = ELF32_ADDR_ERR;

    ELFLoader();
    ~ELFLoader();

    list<ELFTools::alloc> allocations;

    template<typename ptr_inner_type>
    [[nodiscard]]
    ELFTools::Lptr<ptr_inner_type> new_lptr(Elf32_Addr runtime_address) const;
    template<typename ptr_inner_type>
    [[nodiscard]]
    ELFTools::Lptr<ptr_inner_type> new_lptr(ptr_inner_type runtime_address) const;

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
     * Load ELF file code and data into a process' address space and maps it
     * @param file ELF to load
     * @param expected_type
     * @return Loaded ELF, nullptr on error
     */
    ELF* load_elf(const SharedPointer<Dentry>& file, ELF_type expected_type);

    ELF* load_elf(void* buf, ELF_type expected_type);

    ELF* load_libdynlk();

    /**
     * Maps the segments of an ELF into the process' virtual address space
     *
     * @param load_elf ELF to map
     * @param runtime_load_address runtime load address of the lib
     */
    void map_elf(const ELF* load_elf, Elf32_Addr runtime_load_address);

    /**
     * Create a process to run an ELF executable
     *
     * @return program's process
     */
    Process* build_process(int argc, const char** argv, pid_t pid, pid_t ppid, const char** envp, const SharedPointer<Dentry>& file, uint);

    /**
     * Writes argc, argv array pointer, argv pointer array and argv contents to stack
     * @param argc number of arguments
     * @param argv argument list
     * @param envp environment pointers
     * @return ESP in process address space ready to be used
     */
    void* write_args_to_stack(int argc, const char** argv, const char** envp) const;

    /**
     * Process setup last phase: once the main ELF has been loaded, this function executes the remaining setup actions:
     * setup process PDT, page tables, pid, ppid, segment selectors, priority, registers and stack
     * @param argc num args
     * @param argv args
     * @param envp environment pointers
     */
    void finalize_process_setup(int argc, const char** argv, const char** envp);

    void load_elf_code(const ELF* elf, uint load_address, uint runtime_load_address) const;

    void setup_elf_got(const ELF* elf, uint elf_runtime_load_address) const;

    Elf32_Addr get_libdynlk_runtime_address();

    void allocate_stacks();

    void setup_pcb(int argc, const char** argv, const char** envp);

    void setup_pdt();

    ELFTools::Lptr<char> write_auxv(const ELFTools::Lptr<char>& stack_top, void* argv0_runtime_addr, void* random_runtime_addr) const;

    static void write_to_stack(ELFTools::Lptr<char>& stack_top, const void* content, size_t size);
public:
    /**
     * Create a process from an ELF loaded in memory
     * @param pid process ID
     * @param ppid parent process ID
     * @param argc num args
     * @param argv args
     * @param envp environment pointers
     * @param file process main ELF' path
     * @param priority process priority
     * @return process, nullptr if an error occurred
     */
    static Process* setup_elf_process(pid_t pid, pid_t ppid, int argc, const char** argv,
                                      const char** envp, const SharedPointer<Dentry>& file, uint priority);
};


#endif //ELFLOADER_H
