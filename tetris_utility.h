#ifndef TETRIS_UTILITY_H
#define TETRIS_UTILITY_H

// Clean this up later
#define ArraySize(arr) (sizeof(arr) / sizeof(*arr))
#define Assert(expression) if (!(expression)) *(char*)0 = 0
#define RGB(r, g, b) ((r) << 16) | ((g) << 8) | (b)

#define PI     3.14159265f
#define TWO_PI 6.28318531f

#endif