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
         * Finds a free block that is at least size bytes long, matches page_info and respects hint_info
         * @param current search subtree root
         * @param size requested size
         * @param page_info requested page info, the block must have the same
         * @param hint_info allocation hint
         * @return A large enough free block if one has been found, else nullptr
         */
        static Node* find_free_block(Node* current, uint size, const page_info& page_info, const hint_info& hint_info);
        /**
         * Checks whether a node can be reused to satisfy an allocation
         * @param node node to check
         * @param size requested size
         * @param page_info requested page info
         * @return true if node is free, large enough, has the same page info and is not file-backed
         */
        static bool is_reusable(const Node* node, uint size, const page_info& page_info);
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
        // Returns the node whose allocation contains addr, or nullptr
        Node* get_addr_node(uintptr_t addr) const;
        // Returns the node with the lowest start address strictly greater than addr, or nullptr
        Node* get_next_node(uintptr_t addr) const;
        /**
         * Splits a node in two at address at. The node keeps [start, at), a new node is created for [at, end). If the
         * node is file-backed, the new node's file offset is adjusted accordingly
         * @param node node to split
         * @param at split address, must be page aligned and strictly inside the node
         */
        void split_node(Node* node, uintptr_t at);
        /**
         * Finds the used heap block strictly crossed by a range bound. Such a block cannot be split, as free and
         * realloc look it up by its start address
         * @param bound range bound (start or end)
         * @return the node if there is one, nullptr otherwise
         */
        const Node* get_crossed_used_heap_node(uintptr_t bound) const;
        /**
         * Checks whether setting write permission over a range would require splitting node at bound
         * @param bound range bound (start or end)
         * @param write requested write permission
         * @return true if the allocation containing bound is a used heap block, strictly crossed by bound and its
         * write permission differs from the requested one
         */
        bool would_split_used_heap_node(uintptr_t bound, bool write) const;
        /**
         * Clears the PTEs of the pages of a node, then deletes it. Pages are cleared up to the end of the node's last
         * page: a heap block ending in the middle of a page also clears that page
         * @param node node to remove
         * @param process process the node belongs to
         */
        void unmap_node(Node* node, const Process* process);
        /**
         * Checks that a range only covers allocations that user code is allowed to access
         * @param start range start
         * @param end range end
         * @return false if an allocation within the range is not user accessible (e.g. syscall stack), else true
         */
        bool is_range_user_accessible(uintptr_t start, uintptr_t end) const;
    public:
        enum class FreeState
        {
            OK,
            DOUBLE_FREE,
            NOT_FOUND,
            WRONG_KIND // Heap free called on a mapping, or mapping free called on a heap block
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

        /**
         * Allocates a byte-granular heap block. Heap blocks can share pages with each other and offer no alignment
         * guarantee
         */
        void* allocate(uint size, const page_info& page_info, Process* process, const hint_info& hint_info = DEFAULT_HINT_INFO);
        /**
         * Allocates an anonymous mapping of num_pages fresh pages. Free heap blocks are never reused, so the mapping is
         * page aligned and shares no page with any other allocation. The memory is not zeroed by this function
         * @param num_pages number of pages, must not be 0
         * @param page_info page info of the mapping
         * @param process process where the allocation takes place
         * @param hint_info hint to be respected
         * @return mapping start address, or nullptr if no virtual or physical memory is available
         */
        void* map_anonymous(uint num_pages, const page_info& page_info, Process* process, const hint_info& hint_info = DEFAULT_HINT_INFO);
        /**
         * Reallocates a heap block. Mappings cannot be reallocated: FAILED is returned
         */
        [[nodiscard]] ReallocState realloc(uintptr_t address, uint size, Process* process, uintptr_t& new_address);
        /**
         * Frees a heap block. WRONG_KIND is returned if address is the start of a mapping
         */
        [[nodiscard]] FreeState free(uintptr_t address, const Process* process);
        /**
         * Frees the mapping starting at address: clears the PTEs of its pages and removes it. If the mapping was split
         * (partial munmap or mprotect), only the part starting at address is freed. WRONG_KIND is returned if address
         * is the start of a heap block
         * @param address mapping start address
         * @param process process the mapping belongs to
         */
        [[nodiscard]] FreeState free_mapping(uintptr_t address, const Process* process);
        void ensure_validity() const;
        void free_all(const Process* process); // Frees all allocations registered in the tree
        void register_external_allocation(const allocation& allocation); // Registered externally allocated memory
        [[nodiscard]] uint get_total_size() const;
        [[nodiscard]] Optional<allocation> get_addr_alloc(uintptr_t addr) const;
        /**
         * Sets or removes write permission in the policy of allocations within [start, end), splitting allocations
         * that cross range bounds. File mappings rely on it to check permission upon page faults, and policies are
         * reused when growing or reusing allocations, so they must match the actual permission of their pages.
         * Used heap blocks cannot be split, as free and realloc look them up by their start address: if one crosses a
         * range bound and its permission would change, nothing is modified and false is returned
         * @param start range start, page aligned
         * @param end range end, page aligned
         * @param write whether write permission should be granted
         * @return false if the range partially covers a used heap block, else true
         */
        [[nodiscard]] bool set_write_permission(uintptr_t start, uintptr_t end, bool write);
        /**
         * Removes every allocation within [start, end) and clears the PTEs of their pages, splitting allocations that
         * cross range bounds. Nothing is modified and false is returned if the range partially covers a used heap
         * block, or if it covers memory that is not user accessible
         * @param start range start, page aligned
         * @param end range end, page aligned
         * @param process process the allocations belong to
         * @return false if the range was refused, else true. Range may cover no allocation at all
         */
        [[nodiscard]] bool unmap(uintptr_t start, uintptr_t end, const Process* process);
    };
}
