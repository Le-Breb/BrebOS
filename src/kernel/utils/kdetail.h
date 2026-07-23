#pragma once

#include <concepts>

namespace kdetail
{
    template<typename Hash, typename Equal, typename T, typename K>
    concept FindCompatible =
        std::invocable<Hash, const K&> &&
        std::predicate<Equal, const T&, const K&>;
}
