#pragma once

#include <stdarg.h>
#include <stddef.h>

namespace klib {

template<typename T>
T min(T a, T b) { return a < b ? a : b; }

template<typename T>
T max(T a, T b) { return a > b ? a : b; }

template<typename T>
T round_up(T a, T b) { return (a + b - 1) / b * b; }

template<typename T>
T round_down(T a, T b) { return a / b * b; }

template<typename T>
T abs(T a) { return a < 0 ? -a : a; }

}

using klib::max;
using klib::min;
