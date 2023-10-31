#pragma once

#include <math.h>


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define CLAMP(min, x, max) MAX(min, MIN(max, x))

static const double PI = 3.14159265358979323846;

static inline double deg2rad(double deg)
{
    return deg * PI / 180.0;
}
