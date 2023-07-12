#ifndef TETRIS_H
#define TETRIS_H

#include "tetris_types.h"

typedef struct bitmap {
    void* memory;
    i32 width;
    i32 height;
    i32 pitch;
} bitmap;

extern void OnStartup(void);
extern void Update(bitmap* graphicsBuffer, f32 deltaTime);

#endif