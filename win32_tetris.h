#ifndef WIN32_TETRIS_H
#define WIN32_TETRIS_H

#include "tetris_types.h"
#include "tetris_utility.h"


#define SOUND_SAMPLES_PER_SECOND 48000
#define SOUND_BYTES_PER_SAMPLE 4
#define SOUND_BUFFER_SIZE (i32)(2.0f * SOUND_SAMPLES_PER_SECOND)

#define BITMAP_WIDTH  1280
#define BITMAP_HEIGHT 720


typedef struct sytem_time {
    u16 year;
    u16 month;
    u16 dayOfWeek;
    u16 day;
    u16 hour;
    u16 minute;
    u16 second;
    u16 millisecond;
} system_time;


extern system_time EngineGetSystemTime(void);
extern void* EngineReadEntireFile(char* fileName, i32* bytesRead);
extern b32 EngineWriteEntireFile(const char* fileName, const void* buffer, i32 bufferSize);
extern void* EngineAllocate(i32 size);
extern void EngineFree(void* memory);

#endif