#ifndef TETRIS_GRAPHICS_H
#define TETRIS_GRAPHICS_H

#include "tetris.h"


#define RGBToU32(r, g, b) (((r) << 16) | ((g) << 8) | (b))

typedef struct font_t {
    bitmap_buffer spriteSheet;
    i32 sheetWidth;
    i32 sheetHeight;
    i32 spriteWidth;
    i32 spriteHeight;
    char* characters;
    i32 charactersCount;
    i32* widths;
    i32* offsets;
} font_t;

extern void DrawRectangle(bitmap_buffer* bitmapDest, i32 x, i32 y, i32 width, i32 height, u32 colour);
extern bitmap_buffer LoadBMP(const char* filePath);
extern void DrawBitmap(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 x, i32 y, i32 width, u8 opacity);
extern void DrawPartialBitmap(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 destX, i32 destY, i32 sourceX, i32 sourceY, i32 width, i32 height, u8 opacity);
extern void DrawBitmapStupid(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 x, i32 y);
extern void DrawBitmapStupidWithOpacity(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 x, i32 y, u8 opacity);
extern font_t InitFont(const char* filePath, i32 sheetWidth, i32 sheetHeight, const char* characters);
extern void DrawNumber(bitmap_buffer* bitmapDest, font_t* font, i32 number, i32 x, i32 y, i32 spacing, b32 isCentred);
extern void DrawText(bitmap_buffer* bitmapDest, font_t* font, const char* text, i32 x, i32 y, i32 spacing, b32 isCentred);

#endif