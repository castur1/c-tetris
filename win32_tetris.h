#ifndef WIN32_TETRIS_H
#define WIN32_TETRIS_H

#include "tetris_types.h"
#include "tetris_utility.h"

extern void* EngineReadEntireFile(char* fileName, i32* bytesRead);
extern b32 EngineWriteEntireFile(const char* fileName, const void* buffer, i32 bufferSize);
extern void* EngineAllocate(i32 size);
extern void EngineFree(void* memory);

#endif