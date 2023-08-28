#ifndef TETRIS_RANDOM_H
#define TETRIS_RANDOM_H

#include "tetris.h"


extern void RandomInit(void);
extern u32 RandomU32(void);
extern i32 RandomI32(void);
extern i32 RandomI32InRange(i32 min, i32 max);

#endif