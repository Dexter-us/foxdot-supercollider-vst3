#include "../spikes/vst3-callback/sc_pool_capacity.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

// Bounded reproduction of the pinned SC HashTable's linear probe.
// Starting at zero with capacity 31 visits only slots 0 and 1.
std::size_t reachableSlots(std::size_t capacity) {
    const auto tableSize = capacity * 2;
    const auto mask = tableSize - 1;
    std::size_t index = 0, visited = 0;
    do {
        ++visited;
        index = (index + 1) & mask;
    } while (index != 0 && visited <= tableSize);
    return visited;
}

int main() {
    assert(reachableSlots(31) == 2);
    for (const auto requested : {std::size_t(1), std::size_t(31), std::size_t(32),
                                 std::size_t(33), std::size_t(57), std::size_t(64),
                                 std::size_t(2048), std::size_t(2049)}) {
        const auto capacity = scPoolCapacity(requested);
        assert(capacity >= requested);
        assert((capacity & (capacity - 1)) == 0);
        assert(reachableSlots(capacity) == capacity * 2);
    }
    std::puts("SC pool regression passed: raw 31 cycles after 2 slots; rounded capacities traverse the entire table.");
}
