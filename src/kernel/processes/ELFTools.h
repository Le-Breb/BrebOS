#pragma once

#include "../core/memory.h"
#include "ELF_defines.h"
#include "../utils/list.h"


namespace ELFTools
{
    struct alloc
    {
        Memory::allocation alloc_; // Allocation in process
        Elf32_Addr load_addr; // Load address in current process

        alloc(const Memory::allocation& alloc_, Elf32_Addr load_addr) :  alloc_(alloc_), load_addr(load_addr) {}
    };

    /**
     * Utility class "Load Pointer"
     * Allows to conveniently access data at load time, given runtime addresses.
     * The class holds a pointer to a list containing runtime<->load_time addresses correspondances and uses it
     * to convert runtime addresses to load_time addresses
     * @tparam ptr_underlying_type pointer data type to use
     */
    template<typename ptr_underlying_type>
    class Lptr
    {
        using ptr = ptr_underlying_type*;
        ptr p_runtime_ptr = nullptr;
        const list<alloc>* allocations;
    public:

        Lptr(ptr runtime_ptr, const list<alloc>* address_space_manager);

        Lptr& operator=(Elf32_Addr runtime_addr);
        Lptr& operator=(ptr runtime_ptr);
        Lptr operator+(int n) const;
        Lptr operator+(unsigned int n) const;
        Lptr& operator++();
        Lptr& operator--();
        void operator-=(int n);
        Lptr operator-(int n) const;
        size_t operator-(const Lptr& other) const;

        void memset(int c, size_t n) const;
        void memcpy(const void* src, size_t n) const;
        template<typename T>
        void write(const T&& t);

        ptr_underlying_type operator*();
        ptr_underlying_type operator[](uint n);

        template<typename Cast>
        Lptr<Cast> convert_to() const;

        [[nodiscard]]
        ptr get_runtime_ptr() const;
        [[nodiscard]]
        static ptr get_load_addr(void* runtime_ptr, const list<alloc>& allocations);
    };
};

#include "ELFTools.hxx"