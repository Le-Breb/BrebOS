#pragma once
#include "kstring.h"
#include "../utils/list.h"

typedef unsigned int uint;

/**
 * This class allows to load ELFs in GDB at runtime!
 * It works in conjunction with gdb_elf_loader.py
 */
class GDB
{
    struct process_info
    {
        const char* path;
        uint base_address;

        bool operator==(const process_info& other) const {return !strcmp(path, other.path) && base_address == other.base_address;}
    };

    list<process_info> loaded_processes{};

    GDB();

    static const char host_path_prefix[];
    static const char vm_path_prefix[];
    static const void* gdb_struct_addr; // Fixed address where python will read data we write
    static process_info* gdb_struct;


    static void* find_elf_text_base_addr(const char* elf_name);
    static char* add_prefix_to_elf_name(const char* path, const char* prefix);
    bool is_elf_loaded(const char* elf_name) const;
public:
    static GDB* get_instance();

    GDB(const GDB& other) = delete;
    GDB& operator=(const GDB& other) = delete;

    void load_elf(const char* elf_name);
    void unload_elf(const char* elf_name);

    ~GDB() = default;
};