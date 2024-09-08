#pragma once

#include <stddef.h>


template<typename T>
bool array_remove_first(T *array, size_t size, T element)
{
    size_t element_idx = 0;
    while (element_idx < size && array[element_idx] != element) {
        element_idx++;
    }

    if (element_idx == size)
        return false;
    
    for (size_t i = element_idx; i < size - 1; i++)
        array[i] = array[i + 1];
    
    return true;
}
