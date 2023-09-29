#ifndef TETRIS_GRAPHICS_H
#define TETRIS_GRAPHICS_H

#include "tetris.h"


#define RGBToU32(r, g, b) (((r) << 16) | ((g) << 8) | (b))


extern void DrawRectangle(bitmap_buffer* bitmapDest, i32 x, i32 y, i32 width, i32 height, u32 colour);
extern bitmap_buffer LoadBMP(const char* filePath);
extern void DrawBitmap(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 x, i32 y, i32 width, u8 opacity);
extern void DrawPartialBitmap(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 destX, i32 destY, i32 sourceX, i32 sourceY, i32 width, i32 height, u8 opacity);
extern void DrawBitmapStupid(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 x, i32 y);
extern void DrawNumber(bitmap_buffer* bitmapDest, u32 number, i32 x, i32 y, i32 digitWidth, i32 spacing, b32 isCentreAligned, bitmap_buffer* digits);

#endif