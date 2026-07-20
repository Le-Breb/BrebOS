#include "Memtree.h"
#include "kstring.h"
#include "../utils/comparison.h"
#include "../processes/process.h"
#include "SlabAllocator.h"
#include "RawMemory.h"
#include "../processes/scheduler.h"

#define ENSURE_VALIDITY 0

namespace Memory
{
    int MemTree::compare_func(const allocation& a, const allocation& b)
    {
        return a.start == b.start
                   ? 0
                   : a.start > b.start ? 1 : -1;
    }

    MemTree::Node* MemTree::find_free_block(Node* current, uint size, const hint_info& hint_info)
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

    MemTree::Node* MemTree::merge_node_with_free_successor(Node* node)
    {
        if (!node->right)
            return nullptr;

        Node* cur = node->right;

        // Find successor
        while (cur->left)
            cur = cur->left;

        if (!cur->data.is_similar_and_contiguous(node->data))
            return nullptr; // Quit if non-contiguous or non-free

        // Merge
        node->data.end += node_size(cur);
        const allocation node_data = node->data; // Get node data as node->data content may change during deletion
        deleteNode(cur);

        return search(node_data); // Cannot return node directly as deleteNode may have modified the tree
    }

    RBTree<allocation>::Node* MemTree::merge_node_with_free_predecessor(Node* node)
    {
        if (!node->left)
            return nullptr;

        Node* cur = node->left;

        // Find predecessor
        while (cur->right)
            cur = cur->right;

        if (!cur->data.is_similar_and_contiguous(node->data))
            return nullptr; // Quit if non-contiguous or non-free

        // Merge
        cur->data.end += node_size(node);
        const allocation cur_data = cur->data; // Get node cur as cur->data content may change during deletion
        deleteNode(node);

        return search(cur_data); // Cannot return cur directly as deleteNode may have modified the tree
    }

    MemTree::Node* MemTree::node_physical_alloc(uint size, const page_info& page_info, const hint_info& hint_info, Process* process)
    {
        const uint num_pages = ADDR_PAGE(size + PAGE_SIZE - 1);
        const void* alloc = sbrk(num_pages, page_info, hint_info, process);
        if (!alloc)
            return nullptr;

        const uintptr_t start_addr = reinterpret_cast<uintptr_t>(alloc);

        return insert_aux({start_addr, start_addr + num_pages * PAGE_SIZE, page_info, false});
    }

    RBTree<allocation>::Node* MemTree::merge_free_node(Node* node)
    {
        Node* n = node;
        while (n) {node = n; n = merge_node_with_free_predecessor(node);};
        n = node;
        while (n) {node = n; n = merge_node_with_free_successor(node);};

        return node;
    }

    void MemTree::free_node(Node* node, const Process* process)
    {
        node->data.used = false;
        node = merge_free_node(node);

        const uint ns = node_size(node);
        const uint aligned_start = (node->data.start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        const uint aligned_start_page = ADDR_PAGE(aligned_start);
        const uint aligned_end = node->data.end & ~(PAGE_SIZE - 1);
        const uint num_pages = ns >= PAGE_SIZE ? ADDR_PAGE(aligned_end - aligned_start) : 0;

        if (num_pages >= N_FREE_PAGES_THRESHOLD)
        {
            const Node node_cpy = *node; // Copy node before potentially deleting it
            // Shrink block so that its size is now part1_size
            if (const uint part1_size = aligned_start - node->data.start)
            {
                const uint shrink = ns - part1_size;
                shrink_block(node, shrink);
            }
            else // No part1, simply delete node
                deleteNode(node);

            // Create part2 if there is one, using node_cpy since node may be deleted by now
            if (node_cpy.data.end - aligned_end)
                insert({aligned_end, node_cpy.data.end, node_cpy.data.page_info, node_cpy.data.used});

            for (uint i = 0; i < num_pages; i++)
                free_page(PAGE_ADDR(aligned_start_page + i), process);
        }
    }

    void MemTree::ensure_validity_aux(Node* node, Node* tree_root)
    {
        if (!node)
            return;
        ensure_validity_aux_aux(node, tree_root);
        if (node->left)
        {
            if (compare_func(node->data, node->left->data) < 0)
                irrecoverable_error("MemTree: order is wrong");
            ensure_validity_aux(node->left, tree_root);
        }
        if (node->right)
        {
            if (compare_func(node->data, node->right->data) > 0)
                irrecoverable_error("MemTree: order is wrong");
            ensure_validity_aux(node->right, tree_root);
        }
    }

    bool do_overlap(const allocation& alloc1, const allocation& alloc2)
    {
        return alloc1.start < alloc2.end && alloc2.start < alloc1.end;
    }

    void MemTree::ensure_validity_aux_aux(Node* node, const Node* root)
    {
        if (node != root && do_overlap(node->data, root->data))
            irrecoverable_error("memtree overlap detected");
        if (root->left)
            ensure_validity_aux_aux(node, root->left);
        if (root->right)
            ensure_validity_aux_aux(node, root->right);
    }

    MemTree::Node* MemTree::get_lowest_alloc() const
    {
        if (!root)
            return nullptr;

        Node* node = root;
        while (node->left)
            node = node->left;
        return node;
    }

    uint MemTree::get_total_size_aux(const Node* node)
    {
        if (!node)
            return 0;
        return node_size(node) + get_total_size_aux(node->left) + get_total_size_aux(node->right);
    }

    MemTree::Node* MemTree::duplicate_node(const Node* node, Node* parent)
    {
        if (!node)
            return nullptr;
        Node* dup = new (allocator()) Node(node->data);
        dup->parent = parent; // Do NOT set to node->parent as our parent should be the COPY of node->parent
        dup->color = node->color;
        dup->left = duplicate_node(node->left, dup);
        dup->right = duplicate_node(node->right, dup);

        return dup;
    }

    void MemTree::print_out(const Node* node)
    {
        if (!node)
            return;

        print_out(node->left);
        printf("[0x%08x, 0x%08x] %s\n", node->data.start, node->data.end, node->data.used ? "USED" : "FREE");
        print_out(node->right);
    }

    MemTree::MemTree() : RBTree(compare_func,
                                []() {return SlabAllocator<Node>::get_instance()->alloc();},
                                [](Node* node) {SlabAllocator<Node>::get_instance()->free(node);})
    {
    }

    MemTree::MemTree(const MemTree& other) : RBTree(compare_func, other.allocator, other.deallocator)
    {
        root = duplicate_node(other.root, nullptr);
    }

    MemTree& MemTree::operator=(const MemTree& other)
    {
        if (this == &other)
            return *this;

        allocator = other.allocator;
        deallocator = other.deallocator;

        if (root)
            irrecoverable_error("Overwriting a non-empty Memtree with another one. What to do with the original one is unknown for now");
        root = duplicate_node(other.root, nullptr);

        return *this;
    }

    void* MemTree::allocate(uint size, const page_info& page_info, Process* process, const hint_info& hint_info)
    {
        if (busy)
            irrecoverable_error("reentrency detected");
        busy = 4;


        const int policy = page_info.policy;
        if ((policy & PAGE_PRESENT && policy & PAGE_LAZY_ZERO) || !(policy & (PAGE_PRESENT | PAGE_LAZY_ZERO)))
            irrecoverable_error("%s: invalid policy", __PRETTY_FUNCTION__);

        // Find big enough free block
        Node* node = find_free_block(root, size, hint_info);
        if (node == nullptr) // Physically allocate if needed
            if ((node = node_physical_alloc(size, page_info, hint_info, process)) == nullptr)
                {busy=false; return nullptr;}

        const uintptr_t ns = node_size(node);

        if (const uint shrink = ns - size)
        {
            shrink_block(node, shrink);
            insert({node->data.end,node->data.end + shrink, page_info, false});
        }

        node->data.used = true;

#if ENSURE_VALIDITY
        ensure_validity();
#endif

        busy = false;
        return reinterpret_cast<void*>(node->data.start);
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
        Node* node = search({address, address, DEFAULT_K_PAGE_INFO, false});
        if (!node)
            return ReallocState::FAILED;

        const uint ns = node_size(node);
        if (ns == size)
        {
            new_address = address;
#if ENSURE_VALIDITY
            ensure_validity();
#endif
            return ReallocState::OK;
        }
        if (size < ns)
        {
            const uint shrink = ns - size;
            shrink_block(node, shrink);
            insert({node->data.end, node->data.end + shrink, node->data.page_info, false});
            new_address = address;
#if ENSURE_VALIDITY
            ensure_validity();
#endif
            return ReallocState::OK;
        }

        const allocation allocation = node->data;
        void* new_buffer = allocate(size, node->data.page_info, process);
        if (!new_buffer)
            return ReallocState::NOMEM;

        memcpy(new_buffer, reinterpret_cast<void*>(address), ns);
        node = search(allocation); // Search for node, as allocate may have modified the tree
        if (!node)
            irrecoverable_error("%s: node disappeared...", __PRETTY_FUNCTION__);
        free_node(node, process);
        new_address = reinterpret_cast<uintptr_t>(new_buffer);

#if ENSURE_VALIDITY
        ensure_validity();
#endif

        return ReallocState::OK;
    }

    MemTree::FreeState MemTree::free(uintptr_t address, const Process* process)
    {
        if (busy)
            irrecoverable_error("reentrency detected");
        busy = 3;
        // printf_info("called on %x", address);
        if (!address)
            { busy = false; return FreeState::OK; }

        const allocation dummy = {address, 0,DEFAULT_K_PAGE_INFO};

        Node* node = search(dummy);

        if (!node)
            { busy = false; return FreeState::NOT_FOUND; };

        if (node->data.used == false)
            { busy = false; return FreeState::DOUBLE_FREE; }

        free_node(node, process);

#if ENSURE_VALIDITY
        ensure_validity();
#endif

        busy = false;
        return FreeState::OK;
    }

    void MemTree::ensure_validity() const
    {
        check_invariants();
        if (!root)
            return;

        if (root->parent)
            irrecoverable_error("root parent != nullptr");

        if (root->color != BLACK)
            irrecoverable_error("root is red");
#if ENSURE_VALIDITY > 1
        ensure_validity_aux(root, root);
#endif
    }

    void MemTree::free_all(const Process* process)
    {
        if (busy)
            irrecoverable_error("reentrency detected");
        busy = 2;
        while (root)
        {
            Node* lowest_alloc = get_lowest_alloc();
            const uintptr_t region_start = lowest_alloc->data.start;
            uintptr_t region_end = lowest_alloc->data.end;
            Node previous_lowest_alloc = *lowest_alloc;
            deleteNode(lowest_alloc);

            Node* next_lowest_alloc = get_lowest_alloc();
            while (next_lowest_alloc && previous_lowest_alloc.data.is_similar_and_contiguous(next_lowest_alloc->data))
            {
                region_end = next_lowest_alloc->data.end;
                previous_lowest_alloc = *next_lowest_alloc;
                deleteNode(next_lowest_alloc);
                next_lowest_alloc = get_lowest_alloc();
            }

            const uintptr_t region_size = region_end - region_start;
            if (region_size & (PAGE_SIZE - 1) || region_start & (PAGE_SIZE - 1))
                irrecoverable_error("%s: memory region size is not a multiple of PAGE_SIZE or region start is not page aligned", __PRETTY_FUNCTION__);

            const uint num_pages = region_size / PAGE_SIZE;
            const uint region_start_page = ADDR_PAGE(region_start);
            for (uint i = 0; i < num_pages; i++)
                free_page(PAGE_ADDR(region_start_page + i), process);
        }
        busy = false;
    }

    void MemTree::register_external_allocation(const allocation& allocation)
    {
        if (busy)
            irrecoverable_error("reentrency detected");
        busy = 1;
#if not ENSURE_VALIDITY
        Node node = Node(allocation);
        if (root)
            ensure_validity_aux_aux(&node, root);
#endif
        insert(allocation);
#if ENSURE_VALIDITY
        ensure_validity();
#endif
        busy = false;
    }

    uint MemTree::get_total_size() const
    {
        return get_total_size_aux(root);
    }

    bool MemTree::get_addr_alloc(uintptr_t addr, allocation& alloc) const
    {
        const Node* curr = root;

        while (curr)
        {
            if (curr->data.start <= addr && curr->data.end > addr)
            {
                alloc = curr->data;
                return true;
            }
            if (curr->data.end <= addr)
                curr = curr->right;
            else
                curr = curr->left;
        }

        return false;
    }
}
