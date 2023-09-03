#ifndef TETRIS_GRAPHICS_H
#define TETRIS_GRAPHICS_H

#include "tetris.h"


#define OPACITY_DEFAULT -1
#define OPACITY_NONE -2

#define RGBToU32(r, g, b) (((r) << 16) | ((g) << 8) | (b))


extern void DrawRectangle(bitmap_buffer* graphicsBuffer, i32 x, i32 y, i32 width, i32 height, u32 colour);
extern bitmap_buffer LoadBMP(const char* filePath);
extern void DrawBitmap(bitmap_buffer* graphicsBuffer, bitmap_buffer* bitmap, i32 x, i32 y, i32 width, i32 opacity);

#endif