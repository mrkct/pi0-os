#pragma once

#include <stddef.h>
#include <stdio.h>


template<typename T>
size_t array_find(T *array, size_t size, T element)
{
    for (size_t i = 0; i < size; i++) {
        if (array[i] == element)
            return i;
    }

    return size;
}

template<typename T>
bool array_remove_first(T *array, size_t size, T element)
{
    auto element_idx = array_find(array, size, element);
    if (element_idx == size)
        return false;

    for (size_t i = element_idx; i < size - 1; i++)
        array[i] = array[i + 1];
    
    return true;
}

template<typename T>
bool array_swap_remove(T *array, size_t size, T element)
{
    auto element_idx = array_find(array, size, element);
    if (element_idx == size)
        return false;
    
    array[element_idx] = array[size - 1];
    return true;
}