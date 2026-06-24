#include "Memtree.h"
#include "kstring.h"
#include "../utils/comparison.h"
#include "abi-bits/vm-flags.h"
#include "../processes/process.h"
#include "SlabAllocator.h"
#include "RawMemory.h"
#include "../processes/scheduler.h"

#define ENSURE_VALIDITY 0

bool a = false;

namespace Memory
{
    MemTree MemTree::bootstrap_memtree{};

    BST<allocation>::Node* MemTree::find_free_block(Node* current, uint size, const hint_info& hint_info)
    {
        if (!current)
            return nullptr;

        if (!current->data.used && node_size(current) >= size &&
            (hint_info.hint == current->data.start || (current->data.start > hint_info.hint && !hint_info.is_mandatory)))
            return current;

        if (current->data.start > hint_info.hint)
        {
            if (Node* free_block = find_free_block(current->left, size, hint_info); free_block)
                return free_block;
        }
        if (Node* free_block = find_free_block(current->right, size, hint_info); free_block)
            return free_block;
        return nullptr;
    }

    void MemTree::shrink_block(Node* node, uint size)
    {
        node->data.end -= size;
    }

    uintptr_t MemTree::node_size(const Node* node)
    {
        return node->data.end - node->data.start;
    }

    bool MemTree::merge_node_with_free_successor(Node* node)
    {
        if (!node->right)
            return false;

        Node* prev = node;
        Node* cur = node->right;

        // Find successor
        while (cur->left)
        {
            prev = cur;
            cur = cur->left;
        }

        if (!cur->data.is_similar_and_contiguous(node->data))
            return false; // Quit if non-contiguous or non-free

        // Merge
        node->data.end += node_size(cur);

        // Remove merged node
        remove_node(cur, cur == node->right ? &node->right : &prev->left);

        return true;
    }

    bool MemTree::merge_node_with_free_predecessor(Node* node)
    {
        if (!node->left)
            return false;

        Node* prev = node;
        Node* cur = node->left;

        // Find predecessor
        while (cur->right)
        {
            prev = cur;
            cur = cur->right;
        }

        if (!cur->data.is_similar_and_contiguous(node->data))
            return false; // Quit if non-contiguous or non-free

        // Merge
        node->data.start = cur->data.start;

        remove_node(cur, prev == node ? &node->left : &prev->right);

        return true;
    }

    BST<allocation>::Node* MemTree::node_physical_alloc(uint size, const page_info& page_info, const hint_info& hint_info, Process* process)
    {
        constexpr uint MIN_ALLOC_PAGES = 2;
        const uint num_pages_base = ADDR_PAGE(size + PAGE_SIZE - 1);
        const uint num_pages = max(MIN_ALLOC_PAGES, num_pages_base);

        const void* alloc = sbrk(num_pages, page_info, hint_info, process);
        if (!alloc)
            return nullptr;

        const uintptr_t start_addr = reinterpret_cast<uintptr_t>(alloc);
        Node* node = new_node({start_addr, start_addr + num_pages * PAGE_SIZE, page_info, false});
        add_node(node);

        return node;
    }

    BST<allocation>::Node* MemTree::find_allocation_node(uintptr_t address, Node**& node_ptr) const
    {
        const allocation dummy{address, 0, DEFAULT_K_PAGE_INFO};
        return find_node(dummy, compare_func, node_ptr);
    }

    void MemTree::merge_free_node(Node* node)
    {
        while (merge_node_with_free_predecessor(node)) {};
        while (merge_node_with_free_successor(node)) {};
    }

    void MemTree::free_node(Node* node, Node** node_ptr, const Process* process)
    {
        node->data.used = false;
        merge_free_node(node);

        const uint ns = node_size(node);
        const uint aligned_start = (node->data.start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        const uint aligned_end = node->data.end & ~(PAGE_SIZE - 1);
        const uint num_pages = ns > PAGE_SIZE ? ADDR_PAGE(aligned_end - aligned_start) : 0;

        if (num_pages >= N_FREE_PAGES_THRESHOLD)
        {
            if (*node_ptr != node)
                irrecoverable_error("%s: *node_ptr != node", __PRETTY_FUNCTION__);

            if (num_pages * PAGE_SIZE == ns)
                remove_node(node, node_ptr);
            else
            {
                if (const uint part2_size = node->data.end - aligned_end)
                {
                    allocation end_alloc = node->data;
                    end_alloc.start = aligned_end;
                    end_alloc.end  = node->data.end;
                    add_node(new_node(end_alloc));
                }
                if (const uint part1_size = aligned_start - node->data.start)
                {
                    const uint shrink = ns - part1_size;
                    shrink_block(node, shrink);
                }
                else
                    remove_node(node, node_ptr);
            }

            for (uint i = 0; i < num_pages; i++)
                free_page(ADDR_PAGE(aligned_start + i), process);
        }
    }

    BST<allocation>::Node* MemTree::new_node(const allocation& allocation)
    {
        return new (SlabAllocator<Node>::get_instance()->alloc()) Node{allocation, nullptr, nullptr};
    }

    void MemTree::delete_node(Node* node)
    {
        SlabAllocator<Node>::get_instance()->free(node);
    }

    void MemTree::remove_node(Node* node, Node** node_ptr)
    {
        Node* replacement = nullptr;

        if (node->left)
        {
            if (node->right) // Two children
            {
                // Find successor
                Node* prev = node;
                Node* successor = node->right;
                while (successor->left)
                {
                    prev = successor;
                    successor = successor->left;
                }
                // Replace current node data with successor data
                node->data = successor->data;
                // Delete successor
                (prev == node ? node->right : prev->left) = successor->right;
                delete_node(successor);

                return; // Return, as we don't want to delete cur
            }
            // Only left child
            replacement = node->left;
        }
        else if (node->right) // Only right child
            replacement = node->right;
        else
            replacement = nullptr; // Leaf

        *node_ptr = replacement;

        delete_node(node);
    }

    void MemTree::ensure_validity_aux(Node* node, Node* root)
    {
        if (!node)
            return;
        ensure_validity_aux_aux(node, root);
        ensure_validity_aux(node->left, root);
        ensure_validity_aux(node->right, root);
    }

    bool do_overlap(const allocation& alloc1, const allocation& alloc2)
    {
        return  ((alloc1.start >= alloc2.start && alloc1.start < alloc2.end) || (alloc1.end > alloc2.start && alloc1.end <= alloc2.end)) &&
            !(alloc1.start == alloc2.start && alloc1.end == alloc2.end);
    }

    void MemTree::ensure_validity_aux_aux(Node* node, const Node* root)
    {
        if (!root)
            return;

        if (do_overlap(node->data, root->data))
            irrecoverable_error("memtree overlap detected");
        ensure_validity_aux_aux(node, root->left);
        ensure_validity_aux_aux(node, root->right);
    }

    BST<allocation>::Node* MemTree::get_lowest_alloc(Node* node, Node* prev, Node**& node_ptr)
    {
        if (node->left)
            return get_lowest_alloc(node->left, node, node_ptr);

        node_ptr = prev ? &prev->left : &root;
        return node;
    }

    MemTree::MemTree() : BST([](const Memory::allocation& a, const Memory::allocation& b)
    {
        return a.start == b.start
                   ? 0
                   : a.start > b.start ? 1 : -1;
    })
    {

    }

    void* MemTree::allocate(uint size, const page_info& page_info, Process* process, const hint_info& hint_info)
    {
        const int policy = page_info.policy;
        if ((policy & PAGE_PRESENT && policy & PAGE_LAZY_ZERO) || !(policy & (PAGE_PRESENT | PAGE_LAZY_ZERO)))
            irrecoverable_error("%s: invalid policy", __PRETTY_FUNCTION__);

        // Find big enough free block
        Node* node = find_free_block(root, size, hint_info);
        if (node == nullptr) // Physically allocate if needed
            if ((node = node_physical_alloc(size, page_info, hint_info, process)) == nullptr)
                return nullptr;

        const uintptr_t ns = node_size(node);

        if (const uint shrink = ns - size)
        {
            shrink_block(node, shrink);
            add_node(new_node({node->data.end,node->data.end + shrink, page_info, false}));
        }

        node->data.used = true;

#if ENSURE_VALIDITY
        ensure_validity();
#endif

        return reinterpret_cast<void*>(node->data.start);
    }

    allocation* MemTree::find_allocation(uintptr_t address) const
    {
        [[maybe_unused]] Node** node_ptr;
        if (Node* node = find_allocation_node(address, node_ptr))
            return &node->data;

        return nullptr;
    }

    MemTree::ReallocState MemTree::realloc(uintptr_t address, uint size, Process* process, uintptr_t& new_address)
    {
        if (!address) // realloc on null = malloc
        {
            const page_info page_info = process == kernel_process ? DEFAULT_K_PAGE_INFO : DEFAULT_U_PAGE_INFO;
            if (const void* alloc = allocate(size, page_info, process))
            {
                new_address = reinterpret_cast<uintptr_t>(alloc);
                return ReallocState::OK;
            }
            return ReallocState::NOMEM;
        }
        if (!size) // realloc with size 0 = free
        {
            process->free(reinterpret_cast<void*>(address));
            return ReallocState::OK;
        }

        // Get header
        Node** node_ptr;
        Node* node = find_allocation_node(address, node_ptr);
        if (!node)
            return ReallocState::FAILED;

        const uint base_node_size = node_size(node);

        if (size <= base_node_size)
        {
            const uint shrink = base_node_size - size;
            shrink_block(node, shrink);
            add_node(new_node({node->data.end, node->data.end + shrink, node->data.page_info, false}));
            new_address = address;
#if ENSURE_VALIDITY
            ensure_validity();
#endif
            return ReallocState::OK;
        }

        if (base_node_size == size)
        {
            new_address = address;
#if ENSURE_VALIDITY
            ensure_validity();
#endif
            return ReallocState::OK;
        }

        merge_node_with_free_successor(node);
        const uint new_node_size = node_size(node);

        if (size <= new_node_size)
        {
            const uint shrink = new_node_size - size;
            shrink_block(node, shrink);
            add_node(new_node({node->data.end, node->data.end + shrink, node->data.page_info, false}));
            new_address = address;
#if ENSURE_VALIDITY
            ensure_validity();
#endif
            return ReallocState::OK;
        }

        void* new_buffer = allocate(size, node->data.page_info, process);
        if (!new_buffer)
            return ReallocState::NOMEM;

        memcpy(new_buffer, reinterpret_cast<void*>(address), base_node_size);
        free_node(node, node_ptr, process);
        new_address = reinterpret_cast<uintptr_t>(new_buffer);

#if ENSURE_VALIDITY
        ensure_validity();
#endif

        return ReallocState::OK;
    }

    MemTree::FreeState MemTree::free(uintptr_t address, const Process* process)
    {
        // printf_info("called on %x", address);
        if (!address)
            return FreeState::OK;

        const allocation dummy = {address, 0,DEFAULT_K_PAGE_INFO};

        int prev_cmp = 0;
        Node* prev = nullptr;
        Node* cur = root;

        while (cur)
        {
            const int cmp = compare_func(cur->data, dummy);
            if (cmp == 0)
            {
                if (!cur->data.used)
                    return FreeState::DOUBLE_FREE;
                Node** node_ptr = prev ? (prev_cmp > 0 ? &prev->left : &prev->right) : &root;
                free_node(cur, node_ptr, process);

#if ENSURE_VALIDITY
                ensure_validity();
#endif

                return FreeState::OK;
            }
            prev_cmp = cmp;
            prev = cur;
            cur = cmp > 0 ? cur->left : cur->right;
        }

        return FreeState::NOT_FOUND;
    }

    void MemTree::ensure_validity() const
    {
        BST::ensure_validity();
        ensure_validity_aux(root, root);
    }

    void MemTree::free_all(const Process* process)
    {
        while (root)
        {
            Node** node_ptr;
            Node* lowest_alloc = get_lowest_alloc(root, nullptr, node_ptr);
            const uintptr_t region_start = lowest_alloc->data.start;
            uintptr_t region_end = lowest_alloc->data.end;
            remove_node(lowest_alloc, node_ptr);

            const bool there_were_only_one_node = !root;
            Node* next_lowest_alloc = there_were_only_one_node ? nullptr : get_lowest_alloc(root, nullptr, node_ptr);
            bool contiguous = !there_were_only_one_node && next_lowest_alloc->data.start == lowest_alloc->data.end;
            while (contiguous)
            {
                region_end = next_lowest_alloc->data.end;
                remove_node(next_lowest_alloc, node_ptr);
                if (root)
                {
                    next_lowest_alloc = get_lowest_alloc(root, nullptr, node_ptr);
                    contiguous = next_lowest_alloc->data.start == region_end;
                }
                else
                    contiguous = false;
            }

            const uintptr_t region_size = region_end - region_start;
            if (region_size & (PAGE_SIZE - 1))
                irrecoverable_error("%s: memory region size is not a multiple of PAGE_SIZE", __PRETTY_FUNCTION__);

            const uint num_pages = region_size / PAGE_SIZE;
            for (uint i = 0; i < num_pages; i++)
                free_page(region_start + (i << 12), process);
        }
    }

    void MemTree::register_external_allocation(const allocation& allocation)
    {
        Node* node = new_node(allocation);
        ensure_validity_aux_aux(node, root);
        add_node(node);
    }
}
