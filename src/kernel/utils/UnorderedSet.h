#pragma once

#include <functional>
#include "Hash.h"
#include "kstddef.h"
#include "Status.h"
#include "kdetail.h"


template <typename T, uint32_t capacity, typename hash_func = Hash<T>, typename equal_func = std::equal_to<T>>
class UnorderedSet
{
    [[no_unique_address]] hash_func hash;
    [[no_unique_address]] equal_func equal;

    // Source - https://stackoverflow.com/a/19399478
    // Posted by sehe
    // Retrieved 2026-07-22, License - CC BY-SA 3.0

    static constexpr bool is_power_of_2(int v) {
        return v && ((v & (v - 1)) == 0);
    }

    static_assert(is_power_of_2(capacity), "HashMap capacity must be a power of 2");

    static constexpr uint32_t NIL = capacity;

    uint32_t _size = 0;
    struct elem
    {
        alignas(T) std::byte data[sizeof(T)];
        uint32_t next; // next node in this bucket's chain, or in the free list, or NIL

        T* ptr() { return std::launder(reinterpret_cast<T*>(data)); }

        const T* ptr() const { return std::launder(reinterpret_cast<const T*>(data)); }

        T& value() { return *ptr(); }

        const T& value() const { return *ptr(); }
    };
    elem elements[capacity];
    uint32_t bucket_heads[capacity]; // index of first node of each bucket's chain, or NIL
    uint32_t free_list_head;

    template<typename K>
    requires std::invocable<hash_func, const K&>
    size_t bucket(const K& key) const;
    template <typename K>
    requires kdetail::HashCompatible<hash_func, equal_func, T, K>
    uint32_t find_slot(const K& element, uint32_t& out_bucket) const;
    void unlink_and_free(uint32_t b, uint32_t node_index);
    // Finds the first non-empty bucket at index >= b, updating b and n accordingly
    // (b == capacity && n == NIL if none is found).
    static void advance_to_non_empty_bucket(uint32_t& b, uint32_t& n, const uint32_t* bucket_heads);
public:
    template <bool Const>
    class IteratorBase
    {
        using PoolPtr = std::conditional_t<Const, const elem*, elem*>;
        PoolPtr elements;
        const uint32_t* bucket_heads;
        uint32_t bucket_idx;
        uint32_t node_idx;

    public:
        decltype(auto) operator*() const { return elements[node_idx].value(); } // T& or const T&
        decltype(auto) operator->() const { return elements[node_idx].ptr(); }

        template <bool> friend class IteratorBase;

        IteratorBase& operator++()
        {
            node_idx = elements[node_idx].next;
            if (node_idx == NIL)
            {
                ++bucket_idx;
                advance_to_non_empty_bucket(bucket_idx, node_idx, bucket_heads);
            }
            return *this;
        }

        bool operator==(const IteratorBase& other) const { return other.node_idx == node_idx; }

        IteratorBase(uint32_t bucket_idx, uint32_t node_idx, PoolPtr elements, const uint32_t* bucket_heads)
            : elements(elements), bucket_heads(bucket_heads), bucket_idx(bucket_idx), node_idx(node_idx)
        {
        }

        // allow Iterator -> ConstIterator conversion (not the reverse)
        template <bool OtherConst>
            requires (Const && !OtherConst)
        IteratorBase(const IteratorBase<OtherConst>& o)
            : elements(o.elements), bucket_heads(o.bucket_heads), bucket_idx(o.bucket_idx), node_idx(o.node_idx)
        {
        }

        [[nodiscard]] uint32_t get_bucket() const { return bucket_idx; }
        [[nodiscard]] uint32_t get_index() const { return node_idx; }
    };

    using Iterator      = IteratorBase<false>;
    using ConstIterator = IteratorBase<true>;

    UnorderedSet();
    ~UnorderedSet();
    UnorderedSet(const UnorderedSet&) = delete;
    UnorderedSet& operator=(const UnorderedSet&) = delete;
    template <typename... Args>
    [[nodiscard]]
    Status emplace(Args&&... args);
    bool contains(const T& t) const;
    template <typename K>
    requires kdetail::HashCompatible<hash_func, equal_func, T, K>
    bool contains(const K& element) const;
    void erase(const T& element);
    Iterator erase(const Iterator& it);
    template <typename K>
    requires kdetail::HashCompatible<hash_func, equal_func, T, K>
    ConstIterator find(const K& element) const;
    [[nodiscard]]
    uint32_t size() const;
    [[nodiscard]]
    bool is_full() const;

    [[nodiscard]]
    Iterator begin();

    [[nodiscard]]
    ConstIterator begin() const;

    [[nodiscard]]
    Iterator end();

    [[nodiscard]]
    ConstIterator end() const;
};

#include "UnorderedSet.hxx"
