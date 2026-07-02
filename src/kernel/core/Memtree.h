#pragma once

#include <stdint.h>
#include "SlabAllocator.h"

typedef unsigned int uint;

class Process;

namespace Memory
{
    class MemTree
    {
    protected:
        struct Node
        {
            allocation data;
            Node* left = nullptr;
            Node* right = nullptr;
        };

        Node* root = nullptr;

        static int compare_func(const allocation& a, const allocation& b);

        static constexpr uint N_FREE_PAGES_THRESHOLD = 2;
        static Node* find_free_block(Node* current, uint size, const hint_info& hint_info);
        static void shrink_block(Node* node, uint size);
        static uintptr_t node_size(const Node* node);
        static bool merge_node_with_free_successor(Node* node);
        static bool merge_node_with_free_predecessor(Node* node);
        Node* node_physical_alloc(uint size, const Memory::page_info& page_info, const Memory::hint_info& hint_info, Process* process);
        Node* find_allocation_node(uintptr_t address, Node**& node_ptr) const;
        static void merge_free_node(Node* node);
        void free_node(Node* node, Node** node_ptr, const Process* process);
        static Node* new_node(const allocation& allocation);
        static void delete_node(Node* node);
        static void remove_node(Node* node, Node** node_ptr);
        static void ensure_validity_aux(Node* node, Node* tree_root);
        static void ensure_validity_aux_aux(Node* node, const Node* root);
        void add_node(Node* node);
        Node* find_node(const allocation& elem, Node**& node_ptr) const;
        Node* get_lowest_alloc(Node* node, Node* prev, Node**& node_ptr);
    public:
        static bool go;
        enum class FreeState
        {
            OK,
            DOUBLE_FREE,
            NOT_FOUND
        };
        enum class ReallocState
        {
            OK,
            NOMEM,
            FAILED
        };

        MemTree() = default;
        void* allocate(uint size, const page_info& page_info, Process* process, const hint_info& hint_info = DEFAULT_HINT_INFO);
        allocation* find_allocation(uintptr_t address) const;
        ReallocState realloc(uintptr_t address, uint size, Process* process, uintptr_t& new_address);
        [[nodiscard]] FreeState free(uintptr_t address, const Process* process);
        static MemTree bootstrap_memtree;
        void ensure_validity() const;
        void free_all(const Process* process);
        void register_external_allocation(const allocation& allocation);
    };
}
