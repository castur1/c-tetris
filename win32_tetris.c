#include <Windows.h>
#include <dsound.h>
#include "tetris.h"
#pragma comment(lib, "winmm.lib")  // Perhaps I should just add these to additional dependencies instead?
#pragma comment(lib, "dsound.lib") // ...Like, for compatability reasons and stuff

#include <stdio.h> // Debug


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
static win32_bitmap g_bitmapBuffer;
static LPDIRECTSOUNDBUFFER g_secondarySoundBuffer; // Probably shouldn't be global


static int InitDirectSound(HWND window) {
    LPDIRECTSOUND directSound;

    if (FAILED(DirectSoundCreate(0, &directSound, 0))) {
        return 0;
    }

    if (FAILED(directSound->lpVtbl->SetCooperativeLevel(directSound, window, DSSCL_PRIORITY))) {
        return 0;
    }

    WAVEFORMATEX waveFormat = { 0 };
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 2;
    waveFormat.nSamplesPerSec = 44100; // Should this be a function paramter instead?
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    // DSBCAPS_GLOBALFOCUS? Or even DSBCAPS_STICKYFOCUS?
    DSBUFFERDESC primaryBufferDescription = { .dwSize = sizeof(DSBUFFERDESC), .dwFlags = DSBCAPS_PRIMARYBUFFER };
    LPDIRECTSOUNDBUFFER primaryBuffer;
    if (FAILED(directSound->lpVtbl->CreateSoundBuffer(directSound, &primaryBufferDescription, &primaryBuffer, 0))) {
        return 0;
    }

    if (FAILED(primaryBuffer->lpVtbl->SetFormat(primaryBuffer, &waveFormat))) {
        return 0;
    }

    DSBUFFERDESC secondaryBufferDescription = {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = 0,
        .dwBufferBytes = 2 * waveFormat.nAvgBytesPerSec, // Should this be a function paramter instead?
        .lpwfxFormat = &waveFormat
    };
    if (FAILED(directSound->lpVtbl->CreateSoundBuffer(directSound, &secondaryBufferDescription, &g_secondarySoundBuffer, 0))) {
        return 0;
    }
}

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

static void GetMousePosition(HWND window, i32* x, i32* y) {
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(window, &mousePos);
    *x = mousePos.x;
    *y = mousePos.y;
}

static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_QUIT: {
            g_isRunning = false;
        } break;
        case WM_PAINT: { // Yes, this unfortunately needs to be here
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            ivec2 windowDimensions = GetWindowDimensions(window);
            DisplayBitmapInWindow(&g_bitmapBuffer, deviceContext, windowDimensions.x, windowDimensions.y);
            EndPaint(window, &paint);
        } break;
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

    InitDirectSound(window);
    g_secondarySoundBuffer->lpVtbl->Play(g_secondarySoundBuffer, 0, 0, DSBPLAY_LOOPING);

    timeBeginPeriod(1);

    LARGE_INTEGER performanceFrequence;
    QueryPerformanceFrequency(&performanceFrequence);

    i32 refreshRate = 30;
    i32 screenRefreshRate = GetDeviceCaps(deviceContext, VREFRESH);
    if (screenRefreshRate > 1 && screenRefreshRate < refreshRate) { // This is probably unnecessary
        refreshRate = screenRefreshRate;
    }
    f32 secondsPerFrame = 1.0f / refreshRate;

    f32 secondsForLastFrame = secondsPerFrame;

    InitBitmap(&g_bitmapBuffer, 960, 540);

    keyboard_state keyboardState = { 0 };

    OnStartup();

#define BYTES_PER_SAMPLE 4
#define SAMPLES_PER_SECOND 44100
#define TONE_HZ 262
#define SECOND_COUNT 2
    u32 TEST_runningSampleIndex = 0;
    i32 TEST_halfSquareWavePeriod = SAMPLES_PER_SECOND / TONE_HZ / 2;
    i32 TEST_secondaryBufferSize = SECOND_COUNT * SAMPLES_PER_SECOND * BYTES_PER_SAMPLE;

    g_isRunning = true;
    while (g_isRunning) {
        LARGE_INTEGER performanceCountAtStartOfFrame = GetCurrentPerformanceCount();

        ProcessPendingMessages(window, &g_bitmapBuffer, &keyboardState);

        // Should I make this relative to the bitmap instead?
        GetMousePosition(window, &keyboardState.mouseX, &keyboardState.mouseY);

        bitmap graphicsBuffer = {
            .memory = g_bitmapBuffer.memory,
            .width  = g_bitmapBuffer.width,
            .height = g_bitmapBuffer.height,
            .pitch  = g_bitmapBuffer.pitch,
        };

        Update(&graphicsBuffer, &keyboardState, secondsForLastFrame);

#if 1
        DWORD playCursor;
        DWORD writeCursor;
        if (SUCCEEDED(g_secondarySoundBuffer->lpVtbl->GetCurrentPosition(g_secondarySoundBuffer, &playCursor, &writeCursor))) {
            DWORD byteToLock = (TEST_runningSampleIndex * BYTES_PER_SAMPLE) % TEST_secondaryBufferSize; // 4 is bytesPerSample
            DWORD bytesToWrite;
            if (byteToLock > playCursor) {
                bytesToWrite = TEST_secondaryBufferSize - byteToLock + playCursor;
            }
            else {
                bytesToWrite = playCursor - byteToLock;
            }

            VOID* region1;
            DWORD region1Size;
            VOID* region2;
            DWORD region2Size; 

            if (SUCCEEDED(g_secondarySoundBuffer->lpVtbl->Lock(g_secondarySoundBuffer, byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
                i16* sampleOut = region1;
                DWORD regionSampleCount = region1Size / BYTES_PER_SAMPLE;
                for (DWORD sampleIndex = 0; sampleIndex < regionSampleCount; ++sampleIndex) {
                    i16 sampleValue = (TEST_runningSampleIndex++ / TEST_halfSquareWavePeriod) % 2 ? 2000 : -2000; // 8000 is the sound wave amplitude
                    *sampleOut++ = sampleValue;
                    *sampleOut++ = sampleValue;
                }

                sampleOut = region2;
                regionSampleCount = region2Size / BYTES_PER_SAMPLE;
                for (DWORD sampleIndex = 0; sampleIndex < regionSampleCount; ++sampleIndex) {
                    i16 sampleValue = (TEST_runningSampleIndex++ / TEST_halfSquareWavePeriod) % 2 ? 2000 : -2000;
                    *sampleOut++ = sampleValue;
                    *sampleOut++ = sampleValue;
                }
            }
            g_secondarySoundBuffer->lpVtbl->Unlock(g_secondarySoundBuffer, region1, region1Size, region2, region2Size);
        }

#endif

        ivec2 windowDimensions = GetWindowDimensions(window);
        DisplayBitmapInWindow(&g_bitmapBuffer, deviceContext, windowDimensions.x, windowDimensions.y);

        LARGE_INTEGER performanceCountAtEndOfFrame = GetCurrentPerformanceCount();
        f32 secondsElapsedForFrame = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, performanceCountAtEndOfFrame, performanceFrequence);
        if (secondsElapsedForFrame < secondsPerFrame) {
            secondsForLastFrame = secondsPerFrame;
            f32 millisecondsToSleep = 1000 * (secondsPerFrame - secondsElapsedForFrame);
            if (millisecondsToSleep >= 1) {
                Sleep(millisecondsToSleep - 1);
            }
            while (secondsElapsedForFrame < secondsPerFrame) {
                secondsElapsedForFrame = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, GetCurrentPerformanceCount(), performanceFrequence);
            }
        }
        else {
            secondsForLastFrame = secondsElapsedForFrame;
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