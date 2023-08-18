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

static void DrawBitmap(bitmap_buffer* graphicsBuffer, bitmap_buffer* bitmap, i32 x, i32 y) {
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

typedef struct audio_buffer {
    i32 sampleCount;
    i16* samples;
} audio_buffer;

// http://soundfile.sapp.org/doc/WaveFormat/
#pragma pack(push, 1)
typedef struct wav_format {
    u8  chunkID[4];     // "RIFF"
    u32 chunkSize;
    u8  format[4];      // "WAVE"

    u8  subchunk1ID[4]; // "fmt "
    u32 subchunk1Size;
    u16 audioFormat;
    u16 numChannels;
    u32 sampleRate;
    u32 byteRate;
    u16 blockAlign;
    u16 bitsPerSample;

    u8  subchunk2ID[4]; // "data"
    u32 subchunk2Size;
    // Here comes the actual audio data
} wav_format;
#pragma pack(pop)

static audio_buffer LoadWAV(const char* filePath) {
    i32 bytesRead;
    void* contents = EngineReadEntireFile(filePath, &bytesRead);
    if (bytesRead == 0) {
        return (audio_buffer){ 0 };
    }

    wav_format format = *(wav_format*)contents;

    // If the WAV file's format doesn't match the game's internal audio format we don't want it
    // Yes we do, make this more intelligent
    if ((format.chunkID[0] != 'R' || format.chunkID[1] != 'I' || format.chunkID[2] != 'F' || format.chunkID[3] != 'F') || // "RIFF"
        (format.format[0] != 'W' || format.format[1] != 'A' || format.format[2] != 'V' || format.format[3] != 'E')     || // "WAVE"
        (format.audioFormat != 1)                                                                                      || // No compression
        (format.numChannels != 2)                                                                                      || // Stereo audio
        (format.sampleRate != 48000)                                                                                   ||
        (format.bitsPerSample != 16))
    {
        return (audio_buffer){ 0 };
    }

    // We never free the allocated data so this should be fine
    audio_buffer result = { 
        .samples = (u8*)contents + 44, // 44 is the size of the header before the data
        .sampleCount = format.subchunk2Size / 2 // subchunk2Size is in bytes
    };
    return result;
}

static void TEST_renderBackround(bitmap_buffer* graphicsBuffer, i32 xOffset, i32 yOffset) {
    u32* pixel = graphicsBuffer->memory;
    for (i32 y = 0; y < graphicsBuffer->height; ++y) {
        for (i32 x = 0; x < graphicsBuffer->width; ++x) {
            *pixel++ = RGBToU32(x + xOffset, 0, y + yOffset);
        }
    }
}

typedef struct audio_channel {
    i16* samples;
    i32 samplesCount;
    i32 sampleIndex;
    b32 isLooping; // Should this be here?
} audio_channel;

static i32 PlaySound(audio_buffer* audioBuffer, b32 isLooping, audio_channel* channels, i32 channelsCount) {
    for (i32 i = 0; i < channelsCount; ++i) {
        if (!channels[i].samples) {
            channels[i] = (audio_channel){
                .samples = audioBuffer->samples,
                .samplesCount = audioBuffer->sampleCount,
                .sampleIndex = 0,
                .isLooping = isLooping
            };
            return i;
        }
    }
}

static void StopSound(i32 index, audio_channel* channels) {
    channels[index].samples = 0;
}

static void ProcessSound(sound_buffer* soundBuffer, audio_channel* channels, i32 channelCount) {
    i16* samples = soundBuffer->samples;
    for (i32 i = 0; i < soundBuffer->samplesCount; ++i) {
        f32 sampleLeft  = 0.0f;
        f32 sampleRight = 0.0f;
        for (i32 j = 0; j < channelCount; ++j) {
            if (!channels[j].samples) {
                continue;
            }

            sampleLeft  += channels[j].samples[channels[j].sampleIndex++] / 32768.0f;
            sampleRight += channels[j].samples[channels[j].sampleIndex++] / 32768.0f;

            if (channels[j].sampleIndex >= channels[j].samplesCount) {
                if (channels[j].isLooping) {
                    channels[j].sampleIndex -= channels[j].samplesCount;
                }
                else {
                    channels[j].samples = 0;
                }
            }
        }

        *samples++ = Clamp(sampleLeft,  -1.0f, 1.0f) * 32768.0f;
        *samples++ = Clamp(sampleRight, -1.0f, 1.0f) * 32768.0f;
    }
}

#define TEST_AUDIO_CHANNEL_COUNT 8

typedef struct game_state {
    i32 xOffset;
    i32 yOffset;

    audio_channel audioChannels[TEST_AUDIO_CHANNEL_COUNT];

    bitmap_buffer testBitmap1;
    bitmap_buffer testBitmap2;
    audio_buffer testWAVData1;
    audio_buffer testWAVData2;
} game_state;

static game_state g_gameState;

void OnStartup(void) {
    g_gameState.testBitmap1  = LoadBMP("assets/OpacityTest.bmp");
    g_gameState.testBitmap2  = LoadBMP("assets/opacity_test.bmp");
    g_gameState.testWAVData1 = LoadWAV("assets/wav_test3.wav");
    g_gameState.testWAVData2 = LoadWAV("assets/wav_test2.wav");
}

// Is there really a need for sound_buffer? Doesn't audio_buffer suffice?
void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    f32 scrollSpeed = 256.0f;
    g_gameState.xOffset += (keyboardState->d.isDown - keyboardState->a.isDown) * scrollSpeed * deltaTime;
    g_gameState.yOffset += (keyboardState->w.isDown - keyboardState->s.isDown) * scrollSpeed * deltaTime;  

    g_gameState.xOffset = Max(0, g_gameState.xOffset);
    g_gameState.yOffset = Max(0, g_gameState.yOffset);

    TEST_renderBackround(graphicsBuffer, g_gameState.xOffset, g_gameState.yOffset);

    TEST_DrawRectangle(graphicsBuffer, keyboardState->mouseX, keyboardState->mouseY, 10, 10, 0xFFFFFF);

    DrawBitmap(graphicsBuffer, &g_gameState.testBitmap1, 50, 50);
    DrawBitmap(graphicsBuffer, &g_gameState.testBitmap2, g_gameState.testBitmap1.width + 50, 50);

#if 1
    static TEST_initAudio = true;
    if (TEST_initAudio) {
        TEST_initAudio = false;
        PlaySound(&g_gameState.testWAVData1, true, g_gameState.audioChannels, TEST_AUDIO_CHANNEL_COUNT);
    }

    if (keyboardState->a.isDown && keyboardState->a.didChangeState) {
        PlaySound(&g_gameState.testWAVData2, false, g_gameState.audioChannels, TEST_AUDIO_CHANNEL_COUNT);
    }

    if (keyboardState->s.isDown && keyboardState->s.didChangeState) {
        StopSound(0, g_gameState.audioChannels);
    }

    ProcessSound(soundBuffer, g_gameState.audioChannels, TEST_AUDIO_CHANNEL_COUNT);
#else 
    static f32 tSine = 0.0f;
    i16 toneVolume = 4000;
    i32 toneHz = keyboardState->w.isDown ? 523 : 262;
    i32 wavePeriod = soundBuffer->samplesPerSecond / toneHz;

    i16* samples = soundBuffer->samples;
    for (i32 sampleIndex = 0; sampleIndex < soundBuffer->samplesCount; ++sampleIndex) {
        i16 sampleValue = toneVolume * sinf(tSine);
        *samples++ = sampleValue;
        *samples++ = sampleValue;

        tSine += TWO_PI / wavePeriod;
        if (tSine > TWO_PI) {
            tSine -= TWO_PI;
        }
    }
#endif 
}