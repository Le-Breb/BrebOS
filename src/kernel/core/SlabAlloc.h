#pragma once

#include "stdint.h"
#include "SlabAllocator.h"

namespace Memory
{
    // Use of dedicated namespace to prevent flooding namespace Memory
    namespace SlabAllocation
    {
        // Structure of size N
        template <uint32_t N>
        struct TmpData
        {
            char inner[N];
        };

        // Pow2<N> == 2^N
        template <uint32_t N>
        struct Pow2
        {
            static constexpr uint32_t value = Pow2<N - 1>::value * 2;
        };

        template <>
        struct Pow2<1>
        {
            static constexpr uint32_t value = 1;
        };

        // Allocates a slab of size S
        template <uint32_t S>
        void* alloc_slab()
        {
            return SlabAllocator<TmpData<S>>::get_instance()->alloc();
        }

        // Frees a slab of size S
        template <uint32_t S>
        void free_slab(void* data)
        {
            SlabAllocator<TmpData<S>>::get_instance()->free(static_cast<TmpData<S>*>(data));
        }

        // Sequence of numbers
        template <uint32_t... Is>
        struct IndexSequence
        {
        };

        // Generator of sequence [0,N]
        template <uint32_t N, uint32_t... Is>
        struct MakeIndexSequence
            : MakeIndexSequence<N - 1, N, Is...>
        {
        };

        template <uint32_t... Is>
        struct MakeIndexSequence<0, Is...>
        {
            using type = IndexSequence<Is...>;
        };

        template <typename Seq>
        struct AllocationTable;

        template <uint32_t... Is>
        struct AllocationTable<IndexSequence<Is...>>
        {
            using alloc_fn = void* (*)();

            // Array of slab allocators for each pow(Is)
            static constexpr alloc_fn value[] = {
                &alloc_slab<Pow2<Is>::value>...
            };

            static constexpr uint32_t count = sizeof...(Is);
        };

        template <typename Seq>
        struct DeallocationTable;

        template <uint32_t... Is>
        struct DeallocationTable<IndexSequence<Is...>>
        {
            using free_fn = void (*)(void*);

            // Array of slab deallocators for each pow(Is)
            static constexpr free_fn value[] = {
                &free_slab<Pow2<Is>::value>...
            };

            static constexpr uint32_t count = sizeof...(Is);
        };

        // Source - https://stackoverflow.com/a/10143264
        // Posted by Yann Droneaud, modified by community. See post 'Timeline' for change history
        // Retrieved 2026-06-30, License - CC BY-SA 3.0

        /**
         * return the smallest power of two value
         * greater than x
         *
         * Input range:  [2..2147483648]
         * Output range: [2..2147483648]
         *
         */
        __attribute__ ((const))
        inline uint32_t p2(uint32_t x)
        {
            if (x == 1) // I added this as I need the function to work for x == 1
                return 1;
#if 0
            assert(x > 1);
            assert(x <= ((UINT32_MAX / 2) + 1));
#endif

            return 1 << (32 - __builtin_clz(x - 1));
        }

        // We'll use buckets for allocations of size [2^1, 2^8], otherwise fallback to malloc
        using BucketSizes = MakeIndexSequence<8>::type;
        using AllocatorsTable = AllocationTable<BucketSizes>;
        using DeallocatorsTable = DeallocationTable<BucketSizes>;
    }

    // Use extern to prevent including "memory.h"
    extern "C" void* malloc(uint n);
    extern "C" void free(void* ptr);

    inline char* slab_allocate(uint size)
    {
        const uint pow = SlabAllocation::p2(size);
        const uint idx = (32 - __builtin_clz(pow - 1));
        if (idx >= SlabAllocation::AllocatorsTable::count)
            return static_cast<char*>(malloc(size)); // Malloc fallback for big allocations
        return static_cast<char*>(SlabAllocation::AllocatorsTable::value[idx]());
    }

    inline void slab_deallocate(char* data, uint size)
    {
        // Ensure data is not null. This can happen for example in the destructor of a TmpString that has been moved
        if (data == nullptr)
            return;

        const uint pow = SlabAllocation::p2(size);
        const uint idx = (32 - __builtin_clz(pow - 1));
        if (idx >= SlabAllocation::AllocatorsTable::count)
            free(data); // Free fallback for big allocations
        SlabAllocation::DeallocatorsTable::value[idx](data);
    }
}
