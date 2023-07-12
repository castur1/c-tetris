#include <Windows.h>
#include "tetris.h"
#pragma comment(lib, "winmm.lib")

#include <stdio.h> // Debug

#define ArraySize(arr) (sizeof(arr) / sizeof(*arr))

typedef struct win32_bitmap {
    BITMAPINFO info;
    void* memory;
    i32 width;
    i32 height;
    i32 pitch;
} win32_bitmap;

typedef struct ivec2 {
    i32 x;
    i32 y;
} ivec2;

static b32 g_isRunning;

static inline LARGE_INTEGER GetCurrentPerformanceCount(void) {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value;
}

static inline f32 PerformanceCountDiffInSeconds(LARGE_INTEGER startCount, LARGE_INTEGER endCount, LARGE_INTEGER performanceFrequency) {
    return (endCount.QuadPart - startCount.QuadPart) / (f32)performanceFrequency.QuadPart;
}

static ivec2 GetWindowDimensions(HWND window) {
    RECT clientRect;
    GetClientRect(window, &clientRect);
    return (ivec2){ clientRect.right - clientRect.left, clientRect.bottom - clientRect.top };
}

#define BYTES_PER_PIXEL 4
static void InitBitmap(win32_bitmap* bitmap, i32 width, i32 height) {
    if (bitmap->memory != NULL) {
        VirtualFree(bitmap->memory, 0, MEM_RELEASE);
    }

    bitmap->width  = width;
    bitmap->height = height;
    bitmap->pitch  = width * BYTES_PER_PIXEL;

    bitmap->info.bmiHeader = (BITMAPINFOHEADER){
        .biSize = sizeof(bitmap->info.bmiHeader),
        .biWidth = width,
        .biHeight = height,
        .biPlanes = 1,
        .biBitCount = BYTES_PER_PIXEL * 8,
        .biCompression = BI_RGB
    };

    i32 memorySize = width * height * BYTES_PER_PIXEL;
    bitmap->memory = VirtualAlloc(NULL, memorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}
#undef BYTES_PER_PIXEL

#define ADD_BARS 1
static void DisplayBitmapInWindow(const win32_bitmap* bitmap, HDC deviceContext, i32 windowWidth, i32 windowHeight) {
#if ADD_BARS
    f32 bitmapAspectRatio = (f32)bitmap->width / bitmap->height;

    if (windowHeight * bitmapAspectRatio < windowWidth) {
        i32 xOffset = (windowWidth - windowHeight * bitmapAspectRatio) / 2.0f + 1;

        PatBlt(deviceContext, 0, 0, xOffset, windowHeight, BLACKNESS);
        PatBlt(deviceContext, windowWidth - xOffset, 0, xOffset, windowHeight, BLACKNESS);

        SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);
        StretchDIBits(deviceContext, xOffset, 0, windowHeight * bitmapAspectRatio, windowHeight, \
            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
}
    else {
        i32 yOffset = (windowHeight - windowWidth / bitmapAspectRatio) / 2.0f + 1;

        PatBlt(deviceContext, 0, 0, windowWidth, yOffset, BLACKNESS);
        PatBlt(deviceContext, 0, windowHeight - yOffset, windowWidth, windowHeight, BLACKNESS);

        SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);
        StretchDIBits(deviceContext, 0, yOffset, windowWidth, windowWidth / bitmapAspectRatio, \
            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
    }
#else
    SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);
    StretchDIBits(deviceContext, 0, 0, windowWidth, windowHeight, \
        0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
#endif
}
#undef ADD_BARS

static void UpdateKeyboardKey(keyboard_key_state* keyState, b32 isDown) {
    if (keyState->isDown != isDown) {
        keyState->isDown = isDown;
        keyState->didChangeState = true;
    }
}

ProcessPendingMessages(HWND window, win32_bitmap* bitmapBuffer, keyboard_state* keyboardState) {
    for (i32 i = 0; i < ArraySize(keyboardState->keys); ++i) {
        keyboardState->keys[i].didChangeState = false;
    }
    keyboardState->mouseLeft.didChangeState  = false;
    keyboardState->mouseRight.didChangeState = false;

    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_PAINT: {
                PAINTSTRUCT paint;
                HDC deviceContext = BeginPaint(window, &paint);
                ivec2 windowDimensions = GetWindowDimensions(window);
                DisplayBitmapInWindow(bitmapBuffer, deviceContext, windowDimensions.x, windowDimensions.y);
                EndPaint(window, &paint);
            } break;
            case WM_KEYDOWN:
            case WM_KEYUP: {
                b32 wasDown = (message.lParam & (1 << 30)) != 0;
                b32 isDown  = (message.lParam & (1 << 31)) == 0;

                b32 altKeyIsDown = false;
                if (isDown) {
                    altKeyIsDown = (message.lParam & (1 << 29)) != 0;
                }

                if (wasDown != isDown) {
                    switch (message.wParam) {
                        case VK_ESCAPE: {
                            g_isRunning = false;
                        } break;
                        // Is there a better solution?
                        case 'W': {
                            UpdateKeyboardKey(&keyboardState->w, isDown);
                        } break;
                        case 'A': {
                            UpdateKeyboardKey(&keyboardState->a, isDown);
                        } break;
                        case 'S': {
                            UpdateKeyboardKey(&keyboardState->s, isDown);
                        } break;
                        case 'D': {
                            UpdateKeyboardKey(&keyboardState->d, isDown);
                        } break;
                    }
                }
            } break;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP: {
                UpdateKeyboardKey(&keyboardState->mouseLeft, message.wParam & MK_LBUTTON);
            } break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP: {
                UpdateKeyboardKey(&keyboardState->mouseRight, message.wParam & MK_RBUTTON);
            } break;
        }

        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CLOSE || message == WM_DESTROY || message == WM_QUIT) {
        g_isRunning = false;
    }

    return DefWindowProc(window, message, wParam, lParam);
}

int CALLBACK WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prevInstance, _In_ LPSTR cmdLine, _In_ int showCmd) {
    WNDCLASSA windowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = instance,
        .lpszClassName = "tetris window class"
        // hIcon, hCursor
    };
    if (!RegisterClass(&windowClass)) {
        return 1;
    }

    HWND window = CreateWindowEx(0, windowClass.lpszClassName, L"Tetris", WS_OVERLAPPEDWINDOW | WS_VISIBLE, \
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, NULL);
    if (!window) {
        return 1;
    }

    HDC deviceContext = GetDC(window);

    timeBeginPeriod(1);

    LARGE_INTEGER performanceFrequence;
    QueryPerformanceFrequency(&performanceFrequence);

    i32 refreshRate = 30;
    i32 screenRefreshRate = GetDeviceCaps(deviceContext, VREFRESH);
    if (screenRefreshRate > 1 && screenRefreshRate < refreshRate) { // This is probably unnecessary
        refreshRate = screenRefreshRate;
    }
    f32 secondsPerFrame = 1.0f / refreshRate;

    win32_bitmap bitmapBuffer;
    InitBitmap(&bitmapBuffer, 960, 540);

    keyboard_state keyboardState = { 0 };

    OnStartup();

    g_isRunning = true;
    while (g_isRunning) {
        LARGE_INTEGER performanceCountAtStartOfFrame = GetCurrentPerformanceCount();

        ProcessPendingMessages(window, &bitmapBuffer, &keyboardState);

        // Should I make this relative to the bitmap instead?
        POINT mousePos;
        GetCursorPos(&mousePos);
        ScreenToClient(window, &mousePos);
        keyboardState.mouseX = mousePos.x;
        keyboardState.mouseY = mousePos.y;

        bitmap graphicsBuffer = {
            .memory = bitmapBuffer.memory,
            .width  = bitmapBuffer.width,
            .height = bitmapBuffer.height,
            .pitch  = bitmapBuffer.pitch,
        };

        // Assumes secondsPerFrame is hit
        Update(&graphicsBuffer, secondsPerFrame);

        ivec2 windowDimensions = GetWindowDimensions(window);
        DisplayBitmapInWindow(&bitmapBuffer, deviceContext, windowDimensions.x, windowDimensions.y);

        LARGE_INTEGER performanceCountAtEndOfFrame = GetCurrentPerformanceCount();
        f32 secondsElapsedForFrame = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, performanceCountAtEndOfFrame, performanceFrequence);
        if (secondsElapsedForFrame < secondsPerFrame) {
            f32 millisecondsToSleep = 1000 * (secondsPerFrame - secondsElapsedForFrame);
            if (millisecondsToSleep >= 1) {
                Sleep(millisecondsToSleep - 1);
            }
            while (secondsElapsedForFrame < secondsPerFrame) {
                secondsElapsedForFrame = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, GetCurrentPerformanceCount(), performanceFrequence);
            }
        }

#if 1
        f32 debugSeconds = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, GetCurrentPerformanceCount(), performanceFrequence);
        f32 debugFPS = 1.0f / debugSeconds;
        char debugBuffer[64];
        sprintf_s(debugBuffer, 64, "%.2f ms/f, %.2f fps\n", 1000.0f * debugSeconds, debugFPS);
        OutputDebugStringA(debugBuffer);
#endif
    }

    timeEndPeriod(1);

    return 0;
}