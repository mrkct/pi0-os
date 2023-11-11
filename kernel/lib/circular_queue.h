#pragma once

#include <stddef.h>

namespace klib {

template<typename T, size_t Size>
struct CircularQueue {
    size_t first_free, last_occupied;
    bool is_full;
    T data[Size];

    void push(T element) {
        data[first_free] = element;
        first_free = (first_free + 1) % Size;
        is_full = first_free == last_occupied;
    }

    bool pop(T &element) {
        if (!is_full && last_occupied == first_free)
            return false;

        element = data[last_occupied];
        last_occupied = (last_occupied + 1) % Size;
        return true;
    }
};

}
