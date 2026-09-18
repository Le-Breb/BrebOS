#pragma once

#include "UnorderedSet.h"
#include <stdint.h>
#include "optional.h"

[[noreturn]]
extern __attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template<typename K>
requires std::invocable<hash_func, const K&>
size_t UnorderedSet<T, capacity, hash_func, equal_func>::bucket(const K& key) const
{
    return hash(key) & (capacity - 1);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
void UnorderedSet<T, capacity, hash_func, equal_func>::advance_to_non_empty_bucket(
    uint32_t& b, uint32_t& n, const uint32_t* bucket_heads)
{
    while (b < capacity && bucket_heads[b] == NIL)
        ++b;
    n = (b < capacity) ? bucket_heads[b] : NIL;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template <typename K>
requires kdetail::HashCompatible<hash_func, equal_func, T, K>
uint32_t UnorderedSet<T, capacity, hash_func, equal_func>::find_slot(const K& element, uint32_t& out_bucket) const
{
    out_bucket = bucket(element);
    uint32_t cur = bucket_heads[out_bucket];
    while (cur != NIL && !equal(elements[cur].value(), element))
        cur = elements[cur].next;

    return cur;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
void UnorderedSet<T, capacity, hash_func, equal_func>::unlink_and_free(uint32_t b, uint32_t node_index)
{
    uint32_t prev = NIL;
    uint32_t cur = bucket_heads[b];
    while (cur != node_index)
    {
        prev = cur;
        cur = elements[cur].next;
    }

    if (prev == NIL)
        bucket_heads[b] = elements[node_index].next;
    else
        elements[prev].next = elements[node_index].next;

    std::destroy_at(elements[node_index].ptr());

    elements[node_index].next = free_list_head;
    free_list_head = node_index;

    --_size;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
UnorderedSet<T, capacity, hash_func, equal_func>::UnorderedSet()
{
    for (uint32_t i = 0; i < capacity; i++)
    {
        bucket_heads[i] = NIL;
        elements[i].next = (i + 1 < capacity) ? i + 1 : NIL;
    }
    free_list_head = 0;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
UnorderedSet<T, capacity, hash_func, equal_func>::~UnorderedSet()
{
    for (uint32_t b = 0; b < capacity; b++)
    {
        uint32_t cur = bucket_heads[b];
        while (cur != NIL)
        {
            const uint32_t next = elements[cur].next;
            std::destroy_at(elements[cur].ptr());
            cur = next;
        }
    }
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template <typename... Args>
Status UnorderedSet<T, capacity, hash_func, equal_func>::emplace(Args&&... args)
{
    // We need a hashable/comparable T before we know which bucket it belongs to and whether it's
    // already present, so we can't avoid constructing it before that check. But we CAN avoid
    // constructing it twice: build it directly into the head of the free list (its final storage
    // if it ends up being kept) instead of on the stack, and only actually pop it from the free
    // list once we know it's not a duplicate. If it turns out to be a duplicate, it's simply
    // destroyed in place and the free list is untouched, since it was never popped.
    //
    // Edge case: this means the capacity check has to happen *before* the duplicate check, since
    // there has to be a free slot to construct into in the first place. So unlike a naive
    // "check duplicate, then check capacity" ordering, emplacing an element that's already present
    // while the set happens to be completely full now reports "set is full!" instead of silently
    // succeeding as a no-op.
    if (free_list_head == NIL)
        return Status::failure("set is full!");

    const uint32_t slot = free_list_head;
    std::construct_at(elements[slot].ptr(), std::forward<Args>(args)...);

    uint32_t b;
    if (find_slot(elements[slot].value(), b) != NIL)
    {
        std::destroy_at(elements[slot].ptr());
        return Status::success(); // Element already present
    }

    free_list_head = elements[slot].next;
    elements[slot].next = bucket_heads[b];
    bucket_heads[b] = slot;
    _size++;

    return Status::success();
}


template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
bool UnorderedSet<T, capacity, hash_func, equal_func>::contains(const T& t) const
{
    uint32_t b;
    return find_slot(t, b) != NIL;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template <typename K> requires kdetail::HashCompatible<hash_func, equal_func, T, K>
bool UnorderedSet<T, capacity, hash_func, equal_func>::contains(const K& element) const
{
    uint32_t b;
    return find_slot(element, b) != NIL;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
void UnorderedSet<T, capacity, hash_func, equal_func>::erase(const T& element)
{
    uint32_t b;
    const uint32_t node = find_slot(element, b);
    if (node == NIL)
        irrecoverable_error("%s: element is not present", __PRETTY_FUNCTION__);

    unlink_and_free(b, node);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
typename UnorderedSet<T, capacity, hash_func, equal_func>::Iterator UnorderedSet<T, capacity, hash_func, equal_func>::
erase(const Iterator& it)
{
    const uint32_t b = it.get_bucket();
    const uint32_t node = it.get_index();
    uint32_t next_bucket = b;
    uint32_t next_node = elements[node].next; // next node in the same bucket's chain, if any

    unlink_and_free(b, node);

    if (next_node == NIL)
    {
        ++next_bucket;
        advance_to_non_empty_bucket(next_bucket, next_node, bucket_heads);
    }

    return Iterator(next_bucket, next_node, elements, bucket_heads);
}


template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template <typename K>
requires kdetail::HashCompatible<hash_func, equal_func, T, K>
typename UnorderedSet<T, capacity, hash_func, equal_func>::ConstIterator UnorderedSet<
    T, capacity, hash_func, equal_func>::find(const K& element) const
{
    uint32_t b;
    const uint32_t node = find_slot(element, b);
    if (node == NIL)
        return end();

    return ConstIterator(b, node, elements, bucket_heads);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
[[nodiscard]]
uint32_t UnorderedSet<T, capacity, hash_func, equal_func>::size() const
{
    return _size;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
bool UnorderedSet<T, capacity, hash_func, equal_func>::is_full() const
{
    return _size == capacity;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
typename UnorderedSet<T, capacity, hash_func, equal_func>::Iterator UnorderedSet<T, capacity, hash_func, equal_func>::
begin()
{
    uint32_t b = 0, n;
    advance_to_non_empty_bucket(b, n, bucket_heads);
    return Iterator(b, n, elements, bucket_heads);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
typename UnorderedSet<T, capacity, hash_func, equal_func>::ConstIterator UnorderedSet<T, capacity, hash_func, equal_func>::
begin() const
{
    uint32_t b = 0, n;
    advance_to_non_empty_bucket(b, n, bucket_heads);
    return ConstIterator(b, n, elements, bucket_heads);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
typename UnorderedSet<T, capacity, hash_func, equal_func>::Iterator UnorderedSet<T, capacity, hash_func, equal_func>::
end()
{
    return Iterator(capacity, NIL, elements, bucket_heads);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
typename UnorderedSet<T, capacity, hash_func, equal_func>::ConstIterator UnorderedSet<T, capacity, hash_func, equal_func>::
end() const
{
    return ConstIterator(capacity, NIL, elements, bucket_heads);
}
