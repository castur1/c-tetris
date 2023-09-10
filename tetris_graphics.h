#ifndef TETRIS_GRAPHICS_H
#define TETRIS_GRAPHICS_H

#include "tetris.h"


#define RGBToU32(r, g, b) (((r) << 16) | ((g) << 8) | (b))


extern void DrawRectangle(bitmap_buffer* graphicsBuffer, i32 x, i32 y, i32 width, i32 height, u32 colour);
extern bitmap_buffer LoadBMP(const char* filePath);
extern void DrawBitmap(bitmap_buffer* graphicsBuffer, bitmap_buffer* bitmap, i32 x, i32 y, i32 width, i32 opacity);
extern void DrawBitmapStupid(bitmap_buffer* graphicsBuffer, bitmap_buffer* bitmap, i32 x, i32 y);
extern void DrawNumber(bitmap_buffer* graphicsBuffer, u32 number, i32 x, i32 y, i32 digitWidth, i32 spacing, b32 isCentreAligned, bitmap_buffer* digits);

#endif