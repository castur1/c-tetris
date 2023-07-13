#include "tetris.h"

void OnStartup(void) {

}

#define RGB(r, g, b) ((r) << 16) | ((g) << 8) | (b)
extern void Update(bitmap* graphicsBuffer, keyboard_state* keyboardState, f32 deltaTime) {
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
            *pixel++ = RGB(x + xOffset, 0, y + yOffset);
        }
    }
}