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

    uint32_t size = 0;
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
    requires kdetail::FindCompatible<hash_func, equal_func, T, K>
    size_t get_index(const K& element) const;
    static void advance_index(uint32_t& index);
public:
    UnorderedSet() = default;
    ~UnorderedSet();
    UnorderedSet(const UnorderedSet&) = delete;
    UnorderedSet& operator=(const UnorderedSet&) = delete;
    Status add(const T& element);
    bool is_present(const T& t) const;
    void remove(const T& element);
    template <typename K>
    requires kdetail::FindCompatible<hash_func, equal_func, T, K>
    Optional<const T*> find(const K& element) const;
    [[nodiscard]]
    uint32_t get_size() const;
    [[nodiscard]]
    bool is_full() const;

    class Iterator
    {
        elem* elems;
        uint32_t index;

    public:
        T& operator*() const { return elems[index].value(); }
        T* operator->() const { return elems[index].ptr(); }
        Iterator& operator++()
        {
            do { ++index; }
            while (index < capacity && elems[index].used);
            return *this;
        }
        bool operator==(const Iterator& other) const { return other.index == index; }

        Iterator(uint32_t index, elem* elems) : elems(elems), index(index) {}
    };

    [[nodiscard]]
    Iterator begin();

    [[nodiscard]]
    Iterator end();
};

#include "UnorderedSet.hxx"
