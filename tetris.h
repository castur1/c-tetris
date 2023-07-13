#ifndef TETRIS_H
#define TETRIS_H

#include "tetris_types.h"
#include "tetris_utility.h"

typedef struct bitmap {
    void* memory;
    i32 width;
    i32 height;
    i32 pitch;
} bitmap;

typedef struct keyboard_key_state {
    b32 isDown;
    b32 didChangeState;
} keyboard_key_state;

typedef struct keyboard_state {
    union {
        struct {
            keyboard_key_state mouseLeft;
            keyboard_key_state mouseRight;
        };
        keyboard_key_state mouseButtons[2];
    };
    i32 mouseX;
    i32 mouseY;

    union {
        struct {
            keyboard_key_state w;
            keyboard_key_state a;
            keyboard_key_state s;
            keyboard_key_state d;
        };
        keyboard_key_state keys[4];
    };
} keyboard_state;

extern void OnStartup(void);
extern void Update(bitmap* graphicsBuffer, keyboard_state* keyboardState, f32 deltaTime);

#endif