#pragma once

#include <stdarg.h>
#include <stddef.h>


template<typename T>
T min(T a, T b) { return a < b ? a : b; }

template<typename T>
T max(T a, T b) { return a > b ? a : b; }

template<typename T>
T clamp(T low, T x, T high) { return max(low, min(high, x)); }

template<typename T>
T round_up(T a, size_t b) { return (a + b - 1) / b * b; }

template<typename T>
T round_down(T a, size_t b) { return a / b * b; }

template<typename T>
T abs(T a) { return a < 0 ? -a : a; }
