#include "tetris_random.h"


#define MULTIPLIER 1664525
#define INCREMENT 1013904223


static u32 seed;


void RandomInit(void) {
    system_time time = EngineGetSystemTime();
    seed = time.day * time.hour * time.minute * time.second * time.millisecond;
}

u32 RandomU32(void) {
    seed = MULTIPLIER * seed + INCREMENT;
    return seed;
}

i32 RandomI32(void) {
    seed = MULTIPLIER * seed + INCREMENT;
    return *(i32*)&seed;
}

i32 RandomI32InRange(i32 min, i32 max) {
    return min + (RandomU32() % (u32)(max - min + 1));
}