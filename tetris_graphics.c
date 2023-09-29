#include "tetris_graphics.h"

// Move this somewhere else, like a maths file or something
static i32 GetLeastSignificantSetBitIndex(u32 bits) {
    for (int i = 0; i < 32; ++i) {
        if (bits & (1 << i)) {
            return i;
        }
    }
    return -1;
}

void DrawRectangle(bitmap_buffer* bitmapDest, i32 x, i32 y, i32 width, i32 height, u32 colour) {
    i32 xMin = Max(x, 0);
    i32 yMin = Max(y, 0);
    i32 xMax = Min(x + width, bitmapDest->width);
    i32 yMax = Min(y + height, bitmapDest->height);

    u8* row = (u8*)bitmapDest->memory + y * bitmapDest->pitch + x * bitmapDest->bytesPerPixel;
    for (i32 y = yMin; y < yMax; ++y) {
        u32* pixel = row;
        for (i32 x = xMin; x < xMax; ++x) {
            *pixel++ = colour;
        }
        row += bitmapDest->pitch;
    }
}

// https://en.wikipedia.org/wiki/BMP_file_format
#pragma pack(push, 1)
typedef struct bitmap_header {
    u8  fileType[2];
    u32 fileSize;
    u16 reserved1;
    u16 reserved2;
    u32 dataOffset;

    u32 infoHeaderSize;
    i32 width;
    i32 height;
    u16 colourPlaneCount;
    u16 bitsPerPixel;
    u32 compression;
    u32 bitmapSize;
    i32 hResolution;
    i32 vResolution;
    u32 paletteColourCount;
    u32 importantColourCount;

    // Are these always here? They shouldn't be according to the documentation...
    u32 bitmaskRed;
    u32 bitmaskGreen;
    u32 bitmaskBlue;
} bitmap_header;
#pragma pack(pop)

bitmap_buffer LoadBMP(const char* filePath) { 
    i32 bytesRead;
    void* contents = EngineReadEntireFile(filePath, &bytesRead);
    if (bytesRead == 0) {
        return (bitmap_buffer){ 0 };
    }

    bitmap_header* header = (bitmap_header*)contents;

    if ((header->fileType[0] != 'B' || header->fileType[1] != 'M') || \
        (header->bitsPerPixel != 24 && header->bitsPerPixel != 32)) {
        return (bitmap_buffer) { 0 };
    }

    bitmap_buffer bitmap = { 
        .memory        = (u8*)contents + header->dataOffset,
        .width         = header->width,
        .height        = header->height,
        .bytesPerPixel = 4,
        .pitch         = header->width * 4
    };

    if (header->bitsPerPixel == 24) {
        i32 pixelCount = bitmap.width * bitmap.height;
        u32* newMemory = EngineAllocate(4 * pixelCount);
        u32* newMemoryDummy = newMemory;
        for (i32 i = 0; i < 3 * pixelCount; i += 3) {
            *newMemory++ = (*(u32*)((u8*)bitmap.memory + i) & 0x00FFFFFF) | 0xFF000000;
        }
        bitmap.memory = newMemoryDummy;
    }

    u32 bitShiftRed;
    u32 bitShiftGreen;
    u32 bitShiftBlue;
    u32 bitShiftAlpha;
    switch (header->compression) {
    case 0: { // BI_RGB
        bitShiftRed   = 16; // 0x00FF0000
        bitShiftGreen = 8;  // 0x0000FF00
        bitShiftBlue  = 0;  // 0x000000FF
        bitShiftAlpha = 24; // 0xFF000000
    } break;
    case 3:   // BI_BITFIELDS
    case 6: { // BI_ALPHABITFIELDS
        bitShiftRed   = GetLeastSignificantSetBitIndex(header->bitmaskRed);
        bitShiftGreen = GetLeastSignificantSetBitIndex(header->bitmaskGreen);
        bitShiftBlue  = GetLeastSignificantSetBitIndex(header->bitmaskBlue);
        bitShiftAlpha = GetLeastSignificantSetBitIndex(~(header->bitmaskRed | header->bitmaskGreen | header->bitmaskBlue));
    } break;
    default: { // I don't want/need to deal with any other type of compression
        return (bitmap_buffer){ 0 };
    } break;
    }

    u32* pixels = bitmap.memory;
    i32 pixelsCount = bitmap.width * bitmap.height;
    for (i32 i = 0; i < pixelsCount; ++i) {
        *pixels++ = \
            (((*pixels >> bitShiftAlpha) & 0xFF) << 24) | \
            (((*pixels >> bitShiftRed)   & 0xFF) << 16) | \
            (((*pixels >> bitShiftGreen) & 0xFF) << 8)  | \
            (((*pixels >> bitShiftBlue)  & 0xFF) << 0);
    }

    if (header->bitsPerPixel == 24) {
        EngineFree(contents);
    }

    return bitmap;
}

void DrawBitmap(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 x, i32 y, i32 width, u8 opacity) {
    f32 aspectRatio = bitmapSource->width / (f32)bitmapSource->height;
    f32 ratio = bitmapSource->width / (f32)width;

    i32 height = width / aspectRatio;

    i32 xMin = Max(x, 0);
    i32 yMin = Max(y, 0);
    i32 xMax = Min(x + width, bitmapDest->width);
    i32 yMax = Min(y + height, bitmapDest->height);

    i32 xOffset = x < 0 ? -x : 0;
    i32 yOffset = y < 0 ? -y : 0;

    i32 sourceXOffset = xOffset * ratio;
    i32 sourceYOffset = yOffset * ratio;

    u32* rowDest = (u32*)bitmapDest->memory + yMin * bitmapDest->width + xMin;
    u32* source = bitmapSource->memory;
    f64 sourceY = sourceYOffset;
    for (i32 y = yMin; y < yMax; ++y) {
        u32* dest = rowDest;
        f64 sourceIndex = sourceXOffset + (i32)sourceY * bitmapSource->width;
        for (i32 x = xMin; x < xMax; ++x) {
            u32 sc = source[(i32)sourceIndex];
            u8 sa = sc >> 24;
            sa = Min(sa, opacity);

            if (sa == 255) {
                *dest = sc;
            }
            else if (sa) {
                u8 sr = sc >> 16;
                u8 sg = sc >> 8;
                u8 sb = sc;

                u8 dr = *dest >> 16;
                u8 dg = *dest >> 8;
                u8 db = *dest;

                f32 t = sa / 255.0f;
                u32 r = dr + t * (i32)(sr - dr);
                u32 g = dg + t * (i32)(sg - dg);
                u32 b = db + t * (i32)(sb - db);

                *dest = RGBToU32(r, g, b);
            }

            ++dest;
            sourceIndex += ratio;
        }
        rowDest += bitmapDest->width;
        sourceY += ratio;
    }
}

void DrawPartialBitmap(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 destX, i32 destY, i32 sourceX, i32 sourceY, i32 width, i32 height, u8 opacity) {
    width = Min(width, bitmapSource->width - sourceX);
    width = Min(width, bitmapDest->width - destX);
    height = Min(height, bitmapSource->height - sourceY);
    height = Min(height, bitmapDest->height - destY);

    u32* destRow = (u32*)bitmapDest->memory + destY * bitmapDest->width + destX;
    u32* sourceRow = (u32*)bitmapSource->memory + sourceY * bitmapSource->width + sourceX;
    for (i32 y = 0; y < height; ++y) {
        u32* dest = destRow;
        u32* source = sourceRow;
        for (i32 x = 0; x < width; ++x) {
            u32 sc = *source;
            u8 sa = sc >> 24;
            sa = Min(sa, opacity);

            if (sa == 255) {
                *dest = sc;
            }
            else if (sa) {
                u8 sr = sc >> 16;
                u8 sg = sc >> 8;
                u8 sb = sc;

                u8 dr = *dest >> 16;
                u8 dg = *dest >> 8;
                u8 db = *dest;

                f32 t = sa / 255.0f;
                u32 r = dr + t * (i32)(sr - dr);
                u32 g = dg + t * (i32)(sg - dg);
                u32 b = db + t * (i32)(sb - db);

                *dest = RGBToU32(r, g, b);
            }

            ++dest;
            ++source;
        }
        destRow += bitmapDest->width;
        sourceRow += bitmapSource->width;
    }
}

void DrawBitmapStupid(bitmap_buffer* bitmapDest, bitmap_buffer* bitmapSource, i32 x, i32 y) {
    u32* rowDest = (u32*)bitmapDest->memory + y * bitmapDest->width + x;
    u32* source = bitmapSource->memory;
    for (i32 y = 0; y < bitmapSource->height; ++y) {
        u32* pixel = rowDest;
        for (i32 x = 0; x < bitmapSource->width; ++x) {
            *pixel++ = *source++;
        }
        rowDest += bitmapDest->width;
    }
}

void DrawNumber(bitmap_buffer* bitmapDest, u32 number, i32 x, i32 y, i32 digitWidth, i32 spacing, b32 isCentreAligned, bitmap_buffer* digits) {
    i32 digitCount = 1;
    u32 denom = 1;
    u32 numberCopy = number;
    while (numberCopy /= 10) {
        ++digitCount;
        denom *= 10;
    }

    if (isCentreAligned) {
        x -= digitCount * (digitWidth + spacing) / 2;
    }

    for (i32 i = 0; i < digitCount; ++i) {
        DrawBitmap(bitmapDest, &digits[number / denom], x + i * (digitWidth + spacing), y, digitWidth, 255);
        number %= denom;
        denom /= 10;
    }
}