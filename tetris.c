#include "tetris.h"

static void TEST_DrawRectangle(bitmap_buffer* graphicsBuffer, i32 x, i32 y, i32 width, i32 height, u32 colour) {
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

// Move this somewhere else, like a maths file or something
static i32 GetLeastSignificantSetBitIndex(u32 bits) {
    for (int i = 0; i < 32; ++i) {
        if ((bits >> i) & 1) {
            return i;
        }
    }
    return -1;
}

static bitmap_buffer LoadBMP(const char* filePath) { 
    i32 bytesRead = 0;
    void* contents = EngineReadEntireFile(filePath, &bytesRead);
    if (bytesRead == 0) {
        return (bitmap_buffer){ 0 };
    }

    bitmap_header* header = (bitmap_header*)contents;
    Assert(header->bitsPerPixel == 32);

    bitmap_buffer bitmap = { 
        .memory = (u8*)contents + header->dataOffset,
        .width  = header->width,
        .height = header->height,
        .bytesPerPixel = 4,
        .pitch  = header->width * 4
    };

    u32 bitShiftRed;
    u32 bitShiftGreen;
    u32 bitShiftBlue;
    u32 bitShiftAlpha;
    if (header->compression == 0) { // BI_RGB
        bitShiftRed   = 0;  // 0x000000FF
        bitShiftGreen = 8;  // 0x0000FF00
        bitShiftBlue  = 16; // 0x00FF0000
        bitShiftAlpha = 24; // 0xFF000000
    }
    else if (header->compression == 3 || header->compression == 6) { // BI_BITFIELDS or BI_ALPHABITFIELDS
        u32 bitmaskRed   = header->bitmaskRed;
        u32 bitmaskGreen = header->bitmaskGreen;
        u32 bitmaskBlue  = header->bitmaskBlue;
        u32 bitmaskAlpha = ~(bitmaskRed | bitmaskGreen | bitmaskBlue);

        bitShiftRed   = GetLeastSignificantSetBitIndex(bitmaskRed);
        bitShiftGreen = GetLeastSignificantSetBitIndex(bitmaskGreen);
        bitShiftBlue  = GetLeastSignificantSetBitIndex(bitmaskBlue);
        bitShiftAlpha = GetLeastSignificantSetBitIndex(bitmaskAlpha);
    }
    else { // I can't be bothered with more complex compression lol, and the compression should always be BI_ALPHABITFIELDS regardless
        return (bitmap_buffer) { 0 };
    }

    u32* pixels = bitmap.memory;
    for (i32 i = 0; i < bitmap.width * bitmap.height; ++i) {
        *pixels++ = \
            (((*pixels >> bitShiftAlpha) & 0xFF) << 24) | \
            (((*pixels >> bitShiftRed)   & 0xFF) << 16) | \
            (((*pixels >> bitShiftGreen) & 0xFF) << 8)  | \
            (((*pixels >> bitShiftBlue)  & 0xFF) << 0);
    }

    return bitmap;
}

static void TEST_DrawBitmap(bitmap_buffer* graphicsBuffer, bitmap_buffer* bitmap, i32 x, i32 y) {
    i32 xMin = Max(x, 0);
    i32 yMin = Max(y, 0);
    i32 xMax = Min(x + bitmap->width, graphicsBuffer->width);
    i32 yMax = Min(y + bitmap->height, graphicsBuffer->height);

    u8* row = (u8*)graphicsBuffer->memory + y * graphicsBuffer->pitch + x * graphicsBuffer->bytesPerPixel;
    u32* source = bitmap->memory;
    for (i32 y = yMin; y < yMax; ++y) {
        u32* pixel = row;
        for (i32 x = xMin; x < xMax; ++x) {
            *pixel++ = *source++;
        }
        row += graphicsBuffer->pitch;
    }
}

static bitmap_buffer g_testBitmap;

void OnStartup(void) {
    g_testBitmap = LoadBMP("assets/OpacityTest.bmp");
}

void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    static i32 xOffset = 0;
    static i32 yOffset = 0;

    xOffset += 5 * (keyboardState->d.isDown - keyboardState->a.isDown);
    yOffset += 5 * (keyboardState->w.isDown - keyboardState->s.isDown);  

    if (xOffset < 0) {
        xOffset = 0;
    }
    if (yOffset < 0) {
        yOffset = 0;
    }

    u32* pixel = graphicsBuffer->memory;
    for (i32 y = 0; y < graphicsBuffer->height; ++y) {
        for (i32 x = 0; x < graphicsBuffer->width; ++x) {
            *pixel++ = RGBToU32(x + xOffset, 0, y + yOffset);
        }
    }

    TEST_DrawBitmap(graphicsBuffer, &g_testBitmap, 0, 0);

#if 0
    static f32 tSine = 0.0f;
    i16 toneVolume = 4000;
    i32 toneHz = keyboardState->w.isDown ? 523 : 262;
    i32 wavePeriod = soundBuffer->samplesPerSecond / toneHz;

    i16* samples = soundBuffer->samples;
    for (i32 sampleIndex = 0; sampleIndex < soundBuffer->samplesCount; ++sampleIndex) {
        i16 sampleValue = toneVolume * sinf(tSine);
        *samples++ = sampleValue;

        tSine += TWO_PI / wavePeriod;
        if (tSine > TWO_PI) {
            tSine -= TWO_PI;
        }
    }
#endif
}