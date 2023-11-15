#include <Windows.h>
#include <dsound.h>
#include "tetris.h"
#include "win32_tetris.h"
#pragma comment(lib, "winmm.lib")  // Perhaps I should just add these to additional dependencies instead?
#pragma comment(lib, "dsound.lib") // ...Like, for compatability reasons and stuff

// Debug / testing
#include <stdio.h>
#include <math.h>

#include <intrin.h>


typedef struct win32_bitmap {
    BITMAPINFO info;
    void* memory;
    i32 width;
    i32 height;
    i32 pitch;
} win32_bitmap;

typedef struct win32_ivec2 {
    i32 x;
    i32 y;
} win32_ivec2;


static b32 g_isRunning;
static win32_bitmap g_bitmapBuffer;
static HWND g_window;


// Credit: Raymond Chen
static void ToggleFullscreen(HWND window) {
    static WINDOWPLACEMENT g_windowPosition = { sizeof(g_windowPosition) };
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO monitorInfo = { sizeof(monitorInfo) };
        if (GetWindowPlacement(window, &g_windowPosition) && GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, \
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, \
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &g_windowPosition);
        SetWindowPos(window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

void* EngineReadEntireFile(const char* filePath, i32* bytesRead) {
    HANDLE fileHandle = CreateFileA(filePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        *bytesRead = 0;
        return 0;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(fileHandle, &fileSize)) {
        CloseHandle(fileHandle);
        *bytesRead = 0;
        return 0;
    }

    void* fileBuffer = VirtualAlloc(NULL, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!fileBuffer) {
        CloseHandle(fileHandle);
        *bytesRead = 0;
        return 0;
    }

    if (!ReadFile(fileHandle, fileBuffer, fileSize.QuadPart, bytesRead, NULL)) {
        VirtualFree(fileBuffer, 0, MEM_RELEASE);
        CloseHandle(fileHandle);
        *bytesRead = 0;
        return 0;
    }

    CloseHandle(fileHandle);

    return fileBuffer;
}

b32 EngineWriteEntireFile(const char* filePath, const void* buffer, i32 bufferSize) {
    HANDLE fileHandle = CreateFileA(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesWritten;
    if (!WriteFile(fileHandle, buffer, bufferSize, &bytesWritten, NULL)) {
        CloseHandle(fileHandle);
        return false;
    }

    CloseHandle(fileHandle);

    return bytesWritten == bufferSize;
}

void* EngineAllocate(i32 size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void EngineFree(void* memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
}

static void ClearSoundBuffer(LPDIRECTSOUNDBUFFER* soundBuffer) {
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;

    if (SUCCEEDED((*soundBuffer)->lpVtbl->Lock(*soundBuffer, 0, SOUND_BUFFER_SIZE, &region1, &region1Size, &region2, &region2Size, 0))) {
        u8* byte = region1;
        for (u32 i = 0; i < region1Size; ++i) {
            *byte++ = 0;
        }

        byte = region2;
        for (u32 i = 0; i < region2Size; ++i) {
            *byte++ = 0;
        }

        (*soundBuffer)->lpVtbl->Unlock(*soundBuffer, region1, region1Size, region2, region2Size);
    }
}

static b32 InitDirectSound(HWND window, LPDIRECTSOUNDBUFFER* secondarySoundBuffer) {
    LPDIRECTSOUND directSound;

    if (FAILED(DirectSoundCreate(0, &directSound, 0))) {
        return false;
    }

    if (FAILED(directSound->lpVtbl->SetCooperativeLevel(directSound, window, DSSCL_PRIORITY))) {
        return false;
    }

    WAVEFORMATEX waveFormat = { 0 };
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 2;
    waveFormat.nSamplesPerSec = SOUND_SAMPLES_PER_SECOND;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    // DSBCAPS_GLOBALFOCUS? Or even DSBCAPS_STICKYFOCUS?
    DSBUFFERDESC primaryBufferDescription = { .dwSize = sizeof(DSBUFFERDESC), .dwFlags = DSBCAPS_PRIMARYBUFFER };
    LPDIRECTSOUNDBUFFER primaryBuffer;
    if (FAILED(directSound->lpVtbl->CreateSoundBuffer(directSound, &primaryBufferDescription, &primaryBuffer, 0))) {
        return false;
    }

    if (FAILED(primaryBuffer->lpVtbl->SetFormat(primaryBuffer, &waveFormat))) {
        return false;
    }

    DSBUFFERDESC secondaryBufferDescription = {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = 0,
        .dwBufferBytes = SOUND_BUFFER_SIZE,
        .lpwfxFormat = &waveFormat
    };
    if (FAILED(directSound->lpVtbl->CreateSoundBuffer(directSound, &secondaryBufferDescription, secondarySoundBuffer, 0))) {
        return false;
    }

    ClearSoundBuffer(secondarySoundBuffer);

    return true;
}

static void FillSoundBuffer(LPDIRECTSOUNDBUFFER* secondarySoundBuffer, sound_buffer* sourceBuffer, DWORD byteToLock, DWORD bytesToWrite) {
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;

    if (SUCCEEDED((*secondarySoundBuffer)->lpVtbl->Lock(*secondarySoundBuffer, byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
        i16* sourceSample = sourceBuffer->samples;

        i16* destSample = region1;
        DWORD region1SampleCount = region1Size / SOUND_BYTES_PER_SAMPLE;
        for (u32 i = 0; i < region1SampleCount; ++i) {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
        }

        destSample = region2;
        DWORD region2SampleCount = region2Size / SOUND_BYTES_PER_SAMPLE;
        for (u32 i = 0; i < region2SampleCount; ++i) {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
        }

        (*secondarySoundBuffer)->lpVtbl->Unlock(*secondarySoundBuffer, region1, region1Size, region2, region2Size);
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

static win32_ivec2 GetWindowDimensions(HWND window) {
    RECT clientRect;
    GetClientRect(window, &clientRect);
    return (win32_ivec2){ clientRect.right - clientRect.left, clientRect.bottom - clientRect.top };
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

static void DisplayBitmapInWindow(const win32_bitmap* bitmap, HDC deviceContext, i32 windowWidth, i32 windowHeight) { 
#if 0
    SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);
    // SetStretchBltMode(deviceContext, STRETCH_HALFTONE); // Too slow...

    f32 bitmapAspectRatio = (f32)bitmap->width / bitmap->height;
    if (windowHeight * bitmapAspectRatio < windowWidth) {
        i32 xOffset = (windowWidth - windowHeight * bitmapAspectRatio) / 2 + 1;

        PatBlt(deviceContext, 0, 0, xOffset, windowHeight, BLACKNESS);
        PatBlt(deviceContext, windowWidth - xOffset, 0, xOffset, windowHeight, BLACKNESS);
        StretchDIBits(deviceContext, xOffset, 0, windowHeight * bitmapAspectRatio, windowHeight, \
            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
    }
    else {
        i32 yOffset = (windowHeight - windowWidth / bitmapAspectRatio) / 2 + 1;

        PatBlt(deviceContext, 0, 0, windowWidth, yOffset, BLACKNESS);
        PatBlt(deviceContext, 0, windowHeight - yOffset, windowWidth, windowHeight, BLACKNESS);
        StretchDIBits(deviceContext, 0, yOffset, windowWidth, windowWidth / bitmapAspectRatio, \
            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
    }
#else
    f32 aspectRatio = bitmap->width / (f32)bitmap->height;

    i32 newWidth = windowHeight * aspectRatio;
    i32 newHeight = windowHeight;

    if (newWidth >= windowWidth) {
        newWidth = windowWidth;
        newHeight = windowWidth / aspectRatio;
    }

    //if (newWidth > bitmap->width) {
    //    // Upscaling

    //    SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);

    //    if (newWidth < windowWidth) {
    //        i32 xOffset = (windowWidth - newWidth) / 2 + 1;

    //        PatBlt(deviceContext, 0, 0, xOffset, windowHeight, BLACKNESS);
    //        PatBlt(deviceContext, windowWidth - xOffset, 0, xOffset, windowHeight, BLACKNESS);
    //        StretchDIBits(deviceContext, xOffset, 0, newWidth, windowHeight, \
    //            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
    //    }
    //    else {
    //        i32 yOffset = (windowHeight - newHeight) / 2 + 1;

    //        PatBlt(deviceContext, 0, 0, windowWidth, yOffset, BLACKNESS);
    //        PatBlt(deviceContext, 0, windowHeight - yOffset, windowWidth, windowHeight, BLACKNESS);
    //        StretchDIBits(deviceContext, 0, yOffset, windowWidth, newHeight, \
    //            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
    //    }
    //}
    //else {
        // Downscaling

        static u32* memory = 0;
        if (memory == 0) {
            i32 monitorWidth  = GetSystemMetrics(SM_CXSCREEN);
            i32 monitorHeight = GetSystemMetrics(SM_CYSCREEN);
            memory = VirtualAlloc(NULL, monitorWidth * monitorHeight * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        }

        f32 ratio = bitmap->width / (f32)(newWidth + 1);

        u32* destRow = memory;
        u32* source = bitmap->memory;
        for (i32 y = 0; y < newHeight; ++y) { 
            i32 y1 = y * ratio;
            i32 y2 = (y + 1) * ratio;

            u32* sourceRow = source + y1 * bitmap->width;
            u32* dest = destRow;
            for (i32 x = 0; x < newWidth; ++x) {
                i32 x1 = x * ratio;
                i32 x2 = (x + 1) * ratio;

                u32 pixelCount = (x2 - x1) * (y2 - y1);

                if (pixelCount == 1) {
                    *dest++ = sourceRow[x1];
                    continue;
                }

                u32 r = 0;
                u32 g = 0;
                u32 b = 0;

                u32* pixels = sourceRow;
                for (i32 i = y1; i < y2; ++i) {
                    for (i32 j = x1; j < x2; ++j) {
                        u32 c = pixels[j];
                        r += (c >> 16) & 0xFF;
                        g += (c >> 8)  & 0xFF;
                        b += (c >> 0)  & 0xFF;
                    }
                    pixels += bitmap->width;
                }

                r /= pixelCount;
                g /= pixelCount;
                b /= pixelCount;

                *dest++ = (r << 16) | (g << 8) | b;
            }
            destRow += newWidth;
        }

        BITMAPINFO info = {
            .bmiHeader = {
                .biSize = sizeof(bitmap->info.bmiHeader),
                .biWidth = newWidth,
                .biHeight = newHeight,
                .biPlanes = 1,
                .biBitCount = 32,
                .biCompression = BI_RGB
        }
        };

        if (newWidth >= windowWidth) {
            i32 yOffset = (windowHeight - newHeight) / 2;

            PatBlt(deviceContext, 0, 0, windowWidth, yOffset, BLACKNESS);
            PatBlt(deviceContext, 0, windowHeight - yOffset, windowWidth, windowHeight, BLACKNESS);
            SetDIBitsToDevice(deviceContext, 0, yOffset, newWidth, newHeight, 0, 0, 0, newHeight, memory, &info, DIB_RGB_COLORS);
        }
        else {
            i32 xOffset = (windowWidth  - newWidth)  / 2;

            PatBlt(deviceContext, 0, 0, xOffset, windowHeight, BLACKNESS);
            PatBlt(deviceContext, windowWidth - xOffset, 0, windowWidth, windowHeight, BLACKNESS);
            SetDIBitsToDevice(deviceContext, xOffset, 0, newWidth, newHeight, 0, 0, 0, newHeight, memory, &info, DIB_RGB_COLORS);
        }
    //}
#endif
}

static void UpdateKeyboardKey(keyboard_key_state* keyState, b32 isDown) {
    if (keyState->isDown != isDown) {
        keyState->isDown = isDown;
        keyState->didChangeState = true;
    }
}

static void ProcessPendingMessages(HWND window, win32_bitmap* bitmapBuffer, keyboard_state* keyboardState) {
    for (i32 i = 0; i < ArraySize(keyboardState->keys); ++i) {
        keyboardState->keys[i].didChangeState = false;
    }
    keyboardState->mouseLeft.didChangeState  = false;
    keyboardState->mouseRight.didChangeState = false;

    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                b32 wasDown = (message.lParam & (1 << 30)) != 0;
                b32 isDown  = (message.lParam & (1 << 31)) == 0;
                b32 altKeyIsDown = (message.lParam & (1 << 29)) != 0 && isDown;

                if (wasDown != isDown) {
                    switch (message.wParam) {
                        case VK_UP: {
                            UpdateKeyboardKey(&keyboardState->up, isDown);
                        } break;
                        case VK_DOWN: {
                            UpdateKeyboardKey(&keyboardState->down, isDown);
                        } break;
                        case VK_LEFT: {
                            UpdateKeyboardKey(&keyboardState->left, isDown);
                        } break;
                        case VK_RIGHT: {
                            UpdateKeyboardKey(&keyboardState->right, isDown);
                        } break;
                        case 'Z': {
                            UpdateKeyboardKey(&keyboardState->z, isDown);
                        } break;
                        case 'X': {
                            UpdateKeyboardKey(&keyboardState->x, isDown);
                        } break;
                        case 'C': {
                            UpdateKeyboardKey(&keyboardState->c, isDown);
                        } break;
                        case VK_RETURN: {
                            UpdateKeyboardKey(&keyboardState->enter, isDown);
                        } break;
                        case VK_ESCAPE: {
                            UpdateKeyboardKey(&keyboardState->esc, isDown);
                        } break;
                        case 'F': {
                            UpdateKeyboardKey(&keyboardState->f, isDown);
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

static void GetCursorPosition(HWND window, win32_bitmap* bitmap, i32* outX, i32* outY) {
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(window, &mousePos);

    win32_ivec2 windowDimensions = GetWindowDimensions(window);

    f32 bitmapAspectRatio = (f32)bitmap->width / bitmap->height;
    if (windowDimensions.y * bitmapAspectRatio < windowDimensions.x) {
        i32 xOffset = (windowDimensions.x - windowDimensions.y * bitmapAspectRatio) / 2;

        if (mousePos.x < xOffset) {
            mousePos.x = xOffset;
        }
        else if (mousePos.x > windowDimensions.x - xOffset) {
            mousePos.x = windowDimensions.x - xOffset;
        }

        mousePos.x -= xOffset;
        *outX = ((f32)mousePos.x / (windowDimensions.y * bitmapAspectRatio)) * bitmap->width;
        *outY = (1.0f - ((f32)mousePos.y / windowDimensions.y)) * bitmap->height;
        *outX = Clamp(*outX, 0, bitmap->width);
        *outY = Clamp(*outY, 0, bitmap->height);
    }
    else {
        i32 yOffset = (windowDimensions.y - windowDimensions.x / bitmapAspectRatio) / 2;

        if (mousePos.y < yOffset) {
            mousePos.y = yOffset;
        }
        else if (mousePos.y > windowDimensions.y - yOffset) {
            mousePos.y = windowDimensions.y - yOffset;
        }

        mousePos.y -= yOffset;
        *outX = ((f32)mousePos.x / windowDimensions.x) * bitmap->width;
        *outY = (1.0f - ((f32)mousePos.y / ((f32)windowDimensions.x / bitmapAspectRatio))) * bitmap->height;
        *outX = Clamp(*outX, 0, bitmap->width);
        *outY = Clamp(*outY, 0, bitmap->height);
    }
}

u32 DEBUG_microsecondsElapsed;

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
            win32_ivec2 windowDimensions = GetWindowDimensions(window);
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
        .lpszClassName = "tetris window class",
        .hIcon = LoadImageA(0, "assets/graphics/icon.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE),
        .hCursor = LoadImageA(0, MAKEINTRESOURCE(IDC_ARROW), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED),
    };
    if (!RegisterClass(&windowClass)) {
        return 1;
    }

    g_window = CreateWindowEx(0, windowClass.lpszClassName, L"Tetris", WS_OVERLAPPEDWINDOW | WS_VISIBLE, \
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, NULL);
    if (!g_window) {
        return 1;
    }

    LPDIRECTSOUNDBUFFER secondarySoundBuffer;
    if (!InitDirectSound(g_window, &secondarySoundBuffer)) {
        return 1;
    }
    secondarySoundBuffer->lpVtbl->Play(secondarySoundBuffer, 0, 0, DSBPLAY_LOOPING);

    i16* soundSamples = VirtualAlloc(NULL, SOUND_BYTES_PER_SAMPLE * SOUND_BUFFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    i32  soundSafetyBytes = 0.03f * SOUND_SAMPLES_PER_SECOND * SOUND_BYTES_PER_SAMPLE;
    i32  soundRunningByteIndex = 0;
    b32  soundIsValid = false;
    i32  soundBytesPerFrame;

    HDC deviceContext = GetDC(g_window);

    timeBeginPeriod(1);

    LARGE_INTEGER performanceFrequence;
    QueryPerformanceFrequency(&performanceFrequence);

    i32 refreshRate = 60;
    i32 screenRefreshRate = GetDeviceCaps(deviceContext, VREFRESH);
    if (screenRefreshRate > 1 && screenRefreshRate < refreshRate) {
        refreshRate = screenRefreshRate;
    }
    f32 secondsPerFrame = 1.0f / refreshRate;

    f32 secondsForLastFrame = secondsPerFrame;

    soundBytesPerFrame = secondsPerFrame * SOUND_SAMPLES_PER_SECOND * SOUND_BYTES_PER_SAMPLE;

    keyboard_state keyboardState = { 0 };
    keyboardState.isMouseVisible = true;

    InitBitmap(&g_bitmapBuffer, BITMAP_WIDTH, BITMAP_HEIGHT);

    OnStartup();

    g_isRunning = true;
    while (g_isRunning) {
        LARGE_INTEGER performanceCountAtStartOfFrame = GetCurrentPerformanceCount();

        ProcessPendingMessages(g_window, &g_bitmapBuffer, &keyboardState);

        CURSORINFO cursorInfo = { .cbSize = sizeof(CURSORINFO) };
        GetCursorInfo(&cursorInfo);
        b32 isMouseVisible = cursorInfo.flags & CURSOR_SHOWING;

        keyboardState.didMouseMove = false;
        if (keyboardState.isMouseVisible != isMouseVisible) {
            ShowCursor(keyboardState.isMouseVisible);
            keyboardState.mouseLastX = keyboardState.mouseX;
            keyboardState.mouseLastY = keyboardState.mouseY;
            keyboardState.mouseX = -1;
            keyboardState.mouseY = -1;
        }
        else {
            i32 mouseX, mouseY;
            GetCursorPosition(g_window, &g_bitmapBuffer, &mouseX, &mouseY);

            if (keyboardState.isMouseVisible) {
                if (mouseX != keyboardState.mouseX || mouseY != keyboardState.mouseY) {
                    keyboardState.didMouseMove = true;
                    keyboardState.mouseLastX = keyboardState.mouseX;
                    keyboardState.mouseLastY = keyboardState.mouseY;
                    keyboardState.mouseX = mouseX;
                    keyboardState.mouseY = mouseY;
                }
            }
            else {
                if (mouseX != keyboardState.mouseLastX || mouseY != keyboardState.mouseLastY) {
                    keyboardState.didMouseMove = true;
                    keyboardState.mouseLastX = mouseX;
                    keyboardState.mouseLastY = mouseY;
                }
            }
        }

        if (keyboardState.isMouseVisible) {
            GetCursorPosition(g_window, &g_bitmapBuffer, &keyboardState.mouseX, &keyboardState.mouseY);
        }

        DWORD byteToLock = 0;
        DWORD bytesToWrite = 0;
        DWORD playCursor;
        DWORD writeCursor;
        if (secondarySoundBuffer->lpVtbl->GetCurrentPosition(secondarySoundBuffer, &playCursor, &writeCursor) == DS_OK) {
            if (!soundIsValid) {
                soundRunningByteIndex = writeCursor;
                soundIsValid = true;
            }
            byteToLock = soundRunningByteIndex;

            DWORD targetCursor = (writeCursor + soundBytesPerFrame + soundSafetyBytes) % SOUND_BUFFER_SIZE;

            if (byteToLock > targetCursor) {
                bytesToWrite = SOUND_BUFFER_SIZE - byteToLock + targetCursor;
            }
            else {
                bytesToWrite = targetCursor - byteToLock;
            }

            soundRunningByteIndex = (soundRunningByteIndex + bytesToWrite) % SOUND_BUFFER_SIZE;
        }
        else {
            soundIsValid = false;
        }

        sound_buffer soundBuffer = {
            .samples = soundSamples,
            .samplesCount = bytesToWrite / SOUND_BYTES_PER_SAMPLE
        };

        bitmap_buffer graphicsBuffer = {
            .memory = g_bitmapBuffer.memory,
            .width = g_bitmapBuffer.width,
            .height = g_bitmapBuffer.height,
            .pitch = g_bitmapBuffer.pitch,
            .bytesPerPixel = 4
        };

        Update(&graphicsBuffer, &soundBuffer, &keyboardState, secondsForLastFrame);

        if (soundIsValid) {
            FillSoundBuffer(&secondarySoundBuffer, &soundBuffer, byteToLock, bytesToWrite);
        }

        win32_ivec2 windowDimensions = GetWindowDimensions(g_window);
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

        f32 debugElapsed = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, performanceCountAtEndOfFrame, performanceFrequence);
        DEBUG_microsecondsElapsed = 1000000 * debugElapsed;
#if 1
        f32 debugSeconds = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, GetCurrentPerformanceCount(), performanceFrequence);
        f32 debugFPS = 1.0f / debugSeconds;
        char debugBuffer[64];
        sprintf_s(debugBuffer, 64, "%.2f ms/f, %.2f fps, %.2f elapsed\n", 1000.0f * debugSeconds, debugFPS, 1000.0f * debugElapsed);
        OutputDebugStringA(debugBuffer);

#endif
    }

    timeEndPeriod(1);

    return 0;
}


system_time EngineGetSystemTime(void) {
    SYSTEMTIME time;
    GetSystemTime(&time);
    return *(system_time*)&time;
}

void EngineClose(void) {
    g_isRunning = false;
}

void EngineToggleFullscreen(void) {
    ToggleFullscreen(g_window);
}