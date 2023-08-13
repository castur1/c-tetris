#ifndef TETRIS_UTILITY_H
#define TETRIS_UTILITY_H

// Clean this up later
#define ArraySize(arr) (sizeof(arr) / sizeof(*arr))
#define Assert(expression) if (!(expression)) *(char*)0 = 0
#define RGBToU32(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(val, min, max) ((val) < (min) ? (min) : (val) > (max) ? (max) : (val))

#define PI     3.14159265f
#define TWO_PI 6.28318531f

#endif