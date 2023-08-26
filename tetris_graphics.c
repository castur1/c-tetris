#include "tetris_graphics.h"

// Move this somewhere else, like a maths file or something
static i32 GetLeastSignificantSetBitIndex(u32 bits) {
    for (int i = 0; i < 32; ++i) {
        if ((bits >> i) & 1) {
            return i;
        }
    }
    return -1;
}

void DrawRectangle(bitmap_buffer* graphicsBuffer, i32 x, i32 y, i32 width, i32 height, u32 colour) {
    i32 xMin = Max(x, 0);
    i32 yMin = Max(y, 0);
    i32 xMax = Min(x + width, graphicsBuffer->width);
    i32 yMax = Min(y + height, graphicsBuffer->height);

    u8* row = (u8*)graphicsBuffer->memory + y * graphicsBuffer->pitch + x * graphicsBuffer->bytesPerPixel;
    for (i32 y = yMin; y < yMax; ++y) {
        u32* pixel = row;
        for (i32 x = xMin; x < xMax; ++x) {
            *pixel++ = colour;
        }
        row += graphicsBuffer->pitch;
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
        (header->bitsPerPixel != 32)) {
        return (bitmap_buffer) { 0 };
    }

    bitmap_buffer bitmap = { 
        .memory        = (u8*)contents + header->dataOffset,
        .width         = header->width,
        .height        = header->height,
        .bytesPerPixel = 4,
        .pitch         = header->width * 4
    };

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

    return bitmap;
}

void DrawBitmap(bitmap_buffer* graphicsBuffer, bitmap_buffer* bitmap, i32 x, i32 y) {
    i32 xMin = Max(x, 0);
    i32 yMin = Max(y, 0);
    i32 xMax = Min(x + bitmap->width, graphicsBuffer->width);
    i32 yMax = Min(y + bitmap->height, graphicsBuffer->height);

    i32 xOffset = x < 0 ? -x : 0;
    i32 yOffset = y < 0 ? -y : 0;

    u8* rowDest   = (u8*)graphicsBuffer->memory + yMin * graphicsBuffer->pitch + xMin * graphicsBuffer->bytesPerPixel;
    u8* rowSource = (u8*)bitmap->memory + yOffset * bitmap->pitch + xOffset * bitmap->bytesPerPixel;
    for (i32 y = yMin; y < yMax; ++y) {
        u32* dest   = rowDest;
        u32* source = rowSource;
        for (i32 x = xMin; x < xMax; ++x) {
            u32 sa = ((u8*)source)[3];
            u32 sr = ((u8*)source)[2];
            u32 sg = ((u8*)source)[1];
            u32 sb = ((u8*)source)[0];

            u32 dr = ((u8*)dest)[2];
            u32 dg = ((u8*)dest)[1];
            u32 db = ((u8*)dest)[0];

            f32 t = sa / 255.0f;
            u32 r = (1.0f - t) * dr + t * sr;
            u32 g = (1.0f - t) * dg + t * sg;
            u32 b = (1.0f - t) * db + t * sb;

            *dest++ = RGBToU32(r, g, b);
            ++source;
        }
        rowDest   += graphicsBuffer->pitch;
        rowSource += bitmap->pitch;
    }
}