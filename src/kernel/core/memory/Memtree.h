#pragma once

#include <stdint.h>
#include "SlabAllocator.h"
#include "../../utils/RedBlackTree.h"
#include "../../utils/optional.h"

typedef unsigned int uint;

class Process;

namespace Memory
{
    class MemTree : protected RBTree<allocation>
    {
        // Manual lock, present to prevent undesired reentrancy. Should only be temporary before a better sync
        // mechanism is implemented
        int busy = false;
    protected:
        /**
         * Compares two allocations a and b by their .data.start
         * @param a alloc a
         * @param b alloc b
         * @return -1 if lower, 0 if equal, 1 if greater
         */
        static int compare_func(const allocation& a, const allocation& b);

        // Minimum number of free pages required to trigger actual freeing of the memory
        static constexpr uint N_FREE_PAGES_THRESHOLD = 2;
        /**
         * Finds a free block that is at least size bytes long and that respects hint_info
         * @param current search subtree root
         * @param size requested size
         * @param hint_info allocation hint
         * @return A large enough free block if one has been found, else nullptr
         */
        static Node* find_free_block(Node* current, uint size, const hint_info& hint_info);
        /**
         * Shrinks a node size by shrink bytes
         * @param node node to shrink
         * @param size how much to shrink
         */
        static void shrink_block(Node* node, uint size);
        // Gets the size of a node allocation
        static uintptr_t node_size(const Node* node);
        /**
         * Merges a free node with its potential free successors
         * @param node the node to shrink
         * @return Pointer to the merged node. May be different from original pointer as tree may me modified
         */
        Node* merge_node_with_free_successor(Node* node);
        /**
         * Merges a free node with its potential free predecessors
         * @param node the node to shrink
         * @return Pointer to the merged node. May be different from original pointer as tree may me modified
         */
        Node* merge_node_with_free_predecessor(Node* node);
        /**
         * Tries to allocate size bytes and returns the newly created node.
         * @param size size of the new node
         * @param page_info page info of the new allocation
         * @param hint_info hint to be respected
         * @param process process where the allocation takes place
         * @return The new node if allocation was successful, nullptr otherwise
         */
        Node* node_physical_alloc(uint size, const page_info& page_info, const hint_info& hint_info, Process* process);
        /**
         * Merges a free node with its potential free predecessors and successors
         * @param node node to be merged
         * @return Pointer to the merged node. May be different from original pointer as tree may me modified
         */
        Node* merge_free_node(Node* node);
        /**
         * Marks a node as free, merges it with its potential free neighbors and potentially frees the memory
         * @param node node to be freed
         * @param process process where the freeing takes place
         */
        void free_node(Node* node, const Process* process);
        static void ensure_validity_aux(Node* node, Node* tree_root);
        static void ensure_validity_aux_aux(Node* node, const Node* root);
        // Get the allocation with the lowest start address
        Node* get_lowest_alloc() const;
        static uint get_total_size_aux(const Node* node);
        /**
         * Duplicates a node (recursively doing the same for its children)
         * @param node node to be duplicate
         * @param parent COPY of the parent of the node to duplicate
         * @return copy of the node
         */
        Node* duplicate_node(const Node* node, Node* parent);
        // Displays every allocation in postorder
        static void print_out(const Node* node);
    public:
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

        MemTree();
        MemTree(const MemTree& other);
        MemTree& operator=(const MemTree& other);

        void* allocate(uint size, const page_info& page_info, Process* process, const hint_info& hint_info = DEFAULT_HINT_INFO);
        [[nodiscard]] ReallocState realloc(uintptr_t address, uint size, Process* process, uintptr_t& new_address);
        [[nodiscard]] FreeState free(uintptr_t address, const Process* process);
        void ensure_validity() const;
        void free_all(const Process* process); // Frees all allocations registered in the tree
        void register_external_allocation(const allocation& allocation); // Registered externally allocated memory
        [[nodiscard]] uint get_total_size() const;
        [[nodiscard]] Optional<allocation> get_addr_alloc(uintptr_t addr) const;
    };
}
