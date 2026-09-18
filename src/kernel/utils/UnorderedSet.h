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

    uint32_t _size = 0;
    struct elem
    {
        alignas(T) std::byte data[sizeof(T)];
        bool used = false;

        T* ptr() { return std::launder(reinterpret_cast<T*>(data)); }

        const T* ptr() const { return std::launder(reinterpret_cast<const T*>(data)); }

        T& value() { return *ptr(); }

        const T& value() const { return *ptr(); }
    };
    elem elements[capacity];
    template<typename K>
    requires std::invocable<hash_func, const K&>
    size_t bucket(const K& key) const;
    template <typename K>
    requires kdetail::HashCompatible<hash_func, equal_func, T, K>
    size_t get_index(const K& element) const;
    static void advance_index(uint32_t& index);
    void erase_at_index(uint32_t i);
public:
    template <typename ElemPtr>
    class IteratorBase
    {
        ElemPtr elems; // elem* or const elem*
        uint32_t index;

    public:
        decltype(auto) operator*() const { return elems[index].value(); } // T& or const T&
        decltype(auto) operator->() const { return elems[index].ptr(); }

        template <typename> friend class IteratorBase;

        IteratorBase& operator++()
        {
            do { ++index; }
            while (index < capacity && !elems[index].used);
            return *this;
        }

        bool operator==(const IteratorBase& other) const { return other.index == index; }

        IteratorBase(uint32_t index, ElemPtr elems) : elems(elems), index(index)
        {
        }

        // allow Iterator -> ConstIterator conversion (not the reverse)
        template <typename OtherPtr>
            requires std::is_same_v<ElemPtr, const std::remove_pointer_t<OtherPtr>*>
        IteratorBase(const IteratorBase<OtherPtr>& o) : elems(o.elems), index(o.index)
        {
        }

        [[nodiscard]] uint32_t get_index() const { return index; }
    };

    using Iterator      = IteratorBase<elem*>;
    using ConstIterator = IteratorBase<const elem*>;

    UnorderedSet() = default;
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
