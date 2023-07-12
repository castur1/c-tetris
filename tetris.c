#include "tetris.h"

void OnStartup(void) {

}

#define RGB(r, g, b) ((r) << 16) | ((g) << 8) | (b)
extern void Update(bitmap* graphicsBuffer, f32 deltaTime) {
    static i32 offset = 0;
    ++offset;

    u32* pixel = graphicsBuffer->memory;
    for (i32 y = 0; y < graphicsBuffer->height; ++y) {
        for (i32 x = 0; x < graphicsBuffer->width; ++x) {
            *pixel++ = RGB(x + offset, 0, y + offset);
        }
    }
}