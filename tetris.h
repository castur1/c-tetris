#ifndef TETRIS_H
#define TETRIS_H

#include "win32_tetris.h"

#include "tetris_types.h"
#include "tetris_utility.h"
#include <math.h> // Implement this myself?

typedef struct bitmap_buffer {
    void* memory;
    i32 width;
    i32 height;
    i32 pitch; // Is this one even neccessary?
    i32 bytesPerPixel;
} bitmap_buffer;

typedef struct sound_buffer {
    i16* samples;
    i32 samplesCount;
} sound_buffer;

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
    // Is there a better way to do this?
    i32 mouseX;
    i32 mouseY;
    i32 mouseLastX;
    i32 mouseLastY;
    b32 didMouseMove;
    b32 isMouseVisible;

    union {
        struct {
            keyboard_key_state up;
            keyboard_key_state down;
            keyboard_key_state left;
            keyboard_key_state right;
            keyboard_key_state z;
            keyboard_key_state x;
            keyboard_key_state c;
            keyboard_key_state spacebar;

            keyboard_key_state enter;
            keyboard_key_state esc;
            keyboard_key_state f;
        };
        keyboard_key_state keys[11];
    };
} keyboard_state;

extern void OnStartup(void);
extern void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime);

#endif