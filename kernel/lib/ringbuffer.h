#pragma once

#include <stddef.h>
#include <kernel/locking/mutex.h>
#include <kernel/locking/irqlock.h>


template<size_t N, typename T>
struct RingBuffer {
    T data[N];
    size_t read_pos = 0;
    size_t write_pos = 0;
    size_t count = 0;

    bool is_full() const { return count == N; }
    bool is_empty() const { return count == 0; }
    size_t available() const { return N - count; }

    void push(const T& item)
    {
        data[write_pos] = item;
        write_pos = (write_pos + 1) % N;
        
        if (count < N) {
            count++;
        } else {
            // When full, move read position to drop oldest element
            read_pos = (read_pos + 1) % N;
        }
    }

    bool pop(T& out)
    {
        if (is_empty())
            return false;
            
        out = data[read_pos];
        read_pos = (read_pos + 1) % N;
        count--;
        return true;
    }
};
