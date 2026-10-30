#pragma once
#include <cstddef>
#include <limits>

// SC HashTable/IntHashTable double this capacity and probe with
// (index + 1) & (tableSize - 1). A non-power-of-two capacity can
// trap probing in a short cycle even when empty slots remain.
constexpr std::size_t scPoolCapacity(std::size_t required) {
    std::size_t capacity = 1;
    while (capacity < required) {
        if (capacity > std::numeric_limits<std::size_t>::max() / 2)
            throw "SuperCollider pool capacity overflow";
        capacity *= 2;
    }
    return capacity;
}
static_assert(scPoolCapacity(31) == 32);
static_assert(scPoolCapacity(32) == 32);
static_assert(scPoolCapacity(33) == 64);
static_assert(scPoolCapacity(2049) == 4096);
