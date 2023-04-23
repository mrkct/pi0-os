#pragma once

#include <stdarg.h>
#include <stddef.h>

namespace klib {

template<typename T>
T min(T a, T b) { return a < b ? a : b; }

template<typename T>
T max(T a, T b) { return a > b ? a : b; }

}

using klib::max;
using klib::min;
