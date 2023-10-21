#include "tetris.h"
#include "tetris_graphics.h"
#include "tetris_sound.h"
#include "tetris_random.h"


#define AUDIO_CHANNEL_COUNT 32

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20

#define AUTO_MOVE_DELAY 0.2f
#define AUTO_MOVE       0.05f
#define SOFT_DROP       0.033f
#define LOCK_DELAY      0.5f

#define BACKGROUND_MUSIC 0.6f
#define SFX_MOVE         1.0f
#define SFX_ROTATE       1.5f
#define SFX_LOCK         2.0f
#define SFX_LINE_CLEAR   1.5f
#define SFX_HOLD         2.0f
#define SFX_LEVEL_UP     1.5f
#define SFX_SOFT_DROP    0.8f

#define SCORE_SINGLE    40
#define SCORE_DOUBLE    100
#define SCORE_TRIPLE    300
#define SCORE_QUADRUPLE 1200
#define SCORE_SOFT_DROP 1
#define SCORE_HARD_DROP 2


#define PRESSED(key) (key.isDown && key.didChangeState)


typedef enum tetromino_type {
    tetromino_type_empty,
    tetromino_type_I,
    tetromino_type_O,
    tetromino_type_T,
    tetromino_type_S,
    tetromino_type_Z,
    tetromino_type_J,
    tetromino_type_L,
} tetromino_type;

static const u16 TETROMINO_EMPTY[4] = { 0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000 };
static const u16 TETROMINO_I[4]     = { 0b0000111100000000, 0b0100010001000100, 0b0000000011110000, 0b0010001000100010 };
static const u16 TETROMINO_O[4]     = { 0b0000011001100000, 0b0000011001100000, 0b0000011001100000, 0b0000011001100000 };
static const u16 TETROMINO_T[4]     = { 0b0010011100000000, 0b0010011000100000, 0b0000011100100000, 0b0010001100100000 };
static const u16 TETROMINO_S[4]     = { 0b0110001100000000, 0b0010011001000000, 0b0000011000110000, 0b0001001100100000 };
static const u16 TETROMINO_Z[4]     = { 0b0011011000000000, 0b0100011000100000, 0b0000001101100000, 0b0010001100010000 };
static const u16 TETROMINO_J[4]     = { 0b0001011100000000, 0b0110001000100000, 0b0000011101000000, 0b0010001000110000 };
static const u16 TETROMINO_L[4]     = { 0b0100011100000000, 0b0010001001100000, 0b0000011100010000, 0b0011001000100000 };

static const u16* TETROMINOES[8] = {
    TETROMINO_EMPTY,
    TETROMINO_I,
    TETROMINO_O,
    TETROMINO_T,
    TETROMINO_S,
    TETROMINO_Z,
    TETROMINO_J,
    TETROMINO_L
};

typedef struct board_t {
    tetromino_type* tiles;
    i32 width;
    i32 height;
    i32 size;
    i32 x;
    i32 y;
    i32 tileSize;
    i32 widthPx;
    i32 heightPx;
} board_t;

typedef struct tetromino_t {
    tetromino_type type;
    i32 rotation;
    i32 x;
    i32 y;
} tetromino_t;

typedef enum button_state {
    button_state_idle,
    button_state_hover,
    button_state_pressed,
    button_state_held,
    button_state_released
} button_state;

typedef struct button_t {
    i32 x;
    i32 y;
    i32 width;
    i32 height;
    button_state state;
} button_t;

typedef void (*scene_pointer)(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime);

typedef struct game_state {
    scene_pointer currentScene;

    audio_channel audioChannels[AUDIO_CHANNEL_COUNT];
    f32 audioVolume;

    i32 highScore;

    // Scene 1 //

    f32 timerFall;
    f32 timerAutoMoveDelay;
    f32 timerAutoMove;
    f32 timerLockDelay;

    board_t board;
    tetromino_t current;
    tetromino_t next[3];
    tetromino_t hold;
    b32 didUseHoldBox;
    tetromino_type bag[7];
    i32 bagIndex;
    i32 score;
    i32 level;
    i32 lines;

    button_t buttonPause;

    // Scene 2 //

    button_t buttonStart;
    button_t buttonOptions;
    button_t buttonQuit;
    u8 currentButtonIndex;

    // Scene 3 //

    audio_channel tempAudioChannels[AUDIO_CHANNEL_COUNT];

    // Scene 4 //

} game_state;

typedef struct game_data {
    font_t font;

    // Scene 1 //

    bitmap_buffer tetrominoes[8];
    bitmap_buffer tetrominoesUI[8];

    bitmap_buffer background;

    sound_buffer backgroundMusic;
    sound_buffer sfxMove;
    sound_buffer sfxRotate;
    sound_buffer sfxLock;
    sound_buffer sfxLineClear;
    sound_buffer sfxHold;
    sound_buffer sfxLevelUp;
    sound_buffer sfxSoftDrop;

    // Scene 2 //

    // background

    bitmap_buffer buttonStartGame;
    bitmap_buffer buttonOptions;
    bitmap_buffer buttonQuitGame;

    // Scene 3 //

    // Scene 4 //

} game_data;

typedef struct save_data {
    i32 highScore;
} save_data;


static game_state g_state;
static game_data  g_data;


/*
 * Scene 1: Gameplay
 * Scene 2: Main menu
 * Scene 3: Paused
 * Scene 4: Options
 */

static void InitScene1(void);
static void InitScene2(void);
static void InitScene3(void);
static void InitScene4(void);
static void Scene1(bitmap_buffer*, sound_buffer*, keyboard_state*, f32);
static void Scene2(bitmap_buffer*, sound_buffer*, keyboard_state*, f32);
static void Scene3(bitmap_buffer*, sound_buffer*, keyboard_state*, f32);
static void Scene4(bitmap_buffer*, sound_buffer*, keyboard_state*, f32);
static void CloseScene1(void);
static void CloseScene2(void);
static void CloseScene3(void);
static void CloseScene4(void);


static board_t InitBoard(i32 width, i32 height, i32 x, i32 y, i32 tileSize) {
    return (board_t) {
        .tiles    = EngineAllocate(width * height * sizeof(tetromino_type)),
        .width    = width,
        .height   = height,
        .x        = x,
        .y        = y,
        .tileSize = tileSize,
        .size     = width * height,
        .widthPx  = width * tileSize,
        .heightPx = height * tileSize
    };
}

static void SetBoardTileSize(board_t* board, i32 tileSize) {
    board->tileSize = tileSize;
    board->widthPx  = board->width  * tileSize;
    board->heightPx = board->height * tileSize;
}

static void ClearBoard(board_t* board) {
    for (i32 i = 0; i < board->size; ++i) {
        board->tiles[i] = tetromino_type_empty;
    }
}

static tetromino_t InitTetromino(tetromino_type type, i32 rotation, i32 x, i32 y) {
    return (tetromino_t){ .type = type, .rotation = rotation, .x = x, .y = y };
}

static void PlaceTetromino(board_t* board, tetromino_t* tetromino) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 x = tetromino->x + i % 4;
            i32 y = tetromino->y + i / 4;
            board->tiles[y * board->width + x] = tetromino->type;
        }
    }
}

static void DrawTetrominoInScreen(bitmap_buffer* graphicsBuffer, tetromino_t* tetromino, i32 size, bitmap_buffer* sprite, i32 opacity) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 xOffset = (i % 4) * size;
            i32 yOffset = (i / 4) * size;
            DrawBitmap(graphicsBuffer, sprite, tetromino->x + xOffset, tetromino->y + yOffset, size, opacity);
        }
    }
}

static void DrawTetrominoInBoard(bitmap_buffer* graphicsBuffer, board_t* board, tetromino_t* tetromino, bitmap_buffer* sprite, i32 opacity) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 x = tetromino->x + (i % 4);
            i32 y = tetromino->y + (i / 4);
            DrawBitmap(graphicsBuffer, sprite, board->x + x * board->tileSize, board->y + y * board->tileSize, board->tileSize, opacity);
        }
    }
}

static void DrawBoard(bitmap_buffer* graphicsBuffer, board_t* board, bitmap_buffer* sprites) {
    for (i32 y = 0; y < board->height; ++y) {
        for (i32 x = 0; x < board->width; ++x) {
            tetromino_type tile = board->tiles[y * board->width + x];
            if (tile != tetromino_type_empty) {
                DrawBitmap(graphicsBuffer, &sprites[tile], board->x + x * board->tileSize, board->y + y * board->tileSize, board->tileSize, 255);
            }
        }
    }
}

static b32 IsTetrominoPosValid(board_t* board, tetromino_t* tetromino) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 x = tetromino->x + (i % 4);
            i32 y = tetromino->y + (i / 4);
            if (x < 0 || x >= board->width || y < 0 || board->tiles[y * board->width + x] != tetromino_type_empty) {
                return false;
            }
        }
    }

    return true;
}

static i32 ProcessLineClears(board_t* board, tetromino_t* tetromino) {
    i32 lineClearCount = 0;

    i32 y = tetromino->y + 3;
    while (y >= 0 && y >= tetromino->y) {
        b32 isLineClear = true;
        for (i32 x = 0; x < board->width; ++x) {
            if (board->tiles[y * board->width + x] == tetromino_type_empty) {
                isLineClear = false;
                break;
            }
        }

        if (isLineClear) {
            ++lineClearCount;
            for (i32 i = y; i < board->height - 1; ++i) {
                for (i32 j = 0; j < board->width; ++j) {
                    board->tiles[i * board->width + j] = board->tiles[(i + 1) * board->width + j];
                }
            }
        }

        --y;
    }

    return lineClearCount;
}

static void RandomizeBag(tetromino_type* bag) {
    for (i32 i = 0; i < 7;) {
        i32 attempt = RandomI32InRange(1, 7);
        for (i32 j = 0; j < i; ++j) {
            if (bag[j] == attempt) {
                attempt = -1;
                break;
            }
        }
        if (attempt == -1) {
            continue;
        }
        bag[i] = attempt;
        ++i;
    }
}

static tetromino_type GetNextTetrominoFromBag(tetromino_type* bag, i32* bagIndex) {
    tetromino_type result = bag[(*bagIndex)++];

    if (*bagIndex >= 7) {
        *bagIndex = 0;
        RandomizeBag(bag);
    }

    return result;
}

static f32 GetCurrentGravityInSeconds(i32 level) {
    f32 gravityInSeconds = 1.0f;
    f32 base = 0.8f - (level - 1) * 0.007f;
    for (i32 i = 0; i < level - 1; ++i) {
        gravityInSeconds *= base;
    }

    return gravityInSeconds;
}

static save_data ReadSaveData(const char* filePath) {
    i32 bytesRead = 0;
    void* contents = EngineReadEntireFile(filePath, &bytesRead);

    save_data data;
    if (bytesRead != sizeof(save_data)) {
        data = (save_data){ .highScore = 0 };
        EngineWriteEntireFile(filePath, &data, sizeof(save_data));
    }
    else {
        data = *(save_data*)contents;
    }
    
    EngineFree(contents);

    return data;
    
}

static inline b32 IsPointInRect(i32 px, i32 py, i32 rxl, i32 ryl, i32 rxr, i32 ryr) {
    return px >= rxl && px < rxr && py >= ryl && py < ryr;
}

// Wtf even is this lol
static void UpdateButtonState(button_t* button, i32 mouseX, i32 mouseY, keyboard_key_state* mouseButton) {
    if (IsPointInRect(mouseX, mouseY, button->x, button->y, button->x + button->width, button->y + button->height)) {
        if (mouseButton->isDown) {
            if (mouseButton->didChangeState) {
                button->state = button_state_pressed;
            }
            else if (button->state == button_state_held || button->state == button_state_pressed) {
                button->state = button_state_held;
            }
            else {
                button->state = button_state_hover;
            }
        }
        else {
            if (button->state == button_state_held || button->state == button_state_pressed) {
                button->state = button_state_released;
            }
            else {
                button->state = button_state_hover;
            }
        }
    }
    else {
        if (button->state == button_state_held || button->state == button_state_pressed) {
            button->state = button_state_released;
        }
        else {
            button->state = button_state_idle;
        }
    }
}

// SCENE 1 //

static void InitScene1(void) {
    g_data.tetrominoes[1] = LoadBMP("assets/graphics/tetrominoes/tetromino_i.bmp");
    g_data.tetrominoes[2] = LoadBMP("assets/graphics/tetrominoes/tetromino_o.bmp");
    g_data.tetrominoes[3] = LoadBMP("assets/graphics/tetrominoes/tetromino_t.bmp");
    g_data.tetrominoes[4] = LoadBMP("assets/graphics/tetrominoes/tetromino_s.bmp");
    g_data.tetrominoes[5] = LoadBMP("assets/graphics/tetrominoes/tetromino_z.bmp");
    g_data.tetrominoes[6] = LoadBMP("assets/graphics/tetrominoes/tetromino_j.bmp");
    g_data.tetrominoes[7] = LoadBMP("assets/graphics/tetrominoes/tetromino_l.bmp");

    g_data.tetrominoesUI[1] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_I_UI.bmp");
    g_data.tetrominoesUI[2] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_O_UI.bmp");
    g_data.tetrominoesUI[3] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_T_UI.bmp");
    g_data.tetrominoesUI[4] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_S_UI.bmp");
    g_data.tetrominoesUI[5] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_Z_UI.bmp");
    g_data.tetrominoesUI[6] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_J_UI.bmp");
    g_data.tetrominoesUI[7] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_L_UI.bmp");

    g_data.background = LoadBMP("assets/graphics/background3.bmp");

    // Continue working on this (or redo it idk)
    g_data.backgroundMusic = LoadWAV("assets/audio/Tetris3.wav");

    // Look these over
    g_data.sfxMove      = LoadWAV("assets/audio/sfx1.wav");
    g_data.sfxRotate    = LoadWAV("assets/audio/sfx4.wav");
    g_data.sfxLock      = LoadWAV("assets/audio/sfx3.wav");
    g_data.sfxLineClear = LoadWAV("assets/audio/sfx5.wav"); 
    g_data.sfxHold      = LoadWAV("assets/audio/sfx2.wav");
    g_data.sfxLevelUp   = LoadWAV("assets/audio/sfx6.wav");
    g_data.sfxSoftDrop  = LoadWAV("assets/audio/sfx1.wav");

    PlaySound(&g_data.backgroundMusic, true, BACKGROUND_MUSIC, g_state.audioChannels, AUDIO_CHANNEL_COUNT);

    RandomInit();

    g_state.board = InitBoard(BOARD_WIDTH, BOARD_HEIGHT, 735, 90, 45);

    RandomizeBag(g_state.bag);

    g_state.current = InitTetromino(GetNextTetrominoFromBag(g_state.bag, &g_state.bagIndex), 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4);
    g_state.next[0] = InitTetromino(GetNextTetrominoFromBag(g_state.bag, &g_state.bagIndex), 0, 1298, 788);
    g_state.next[1] = InitTetromino(GetNextTetrominoFromBag(g_state.bag, &g_state.bagIndex), 0, 1298, 653);
    g_state.next[2] = InitTetromino(GetNextTetrominoFromBag(g_state.bag, &g_state.bagIndex), 0, 1298, 518);
    g_state.hold = InitTetromino(tetromino_type_empty, 0, 533, 788);

    g_state.score = 0;
    g_state.level = 1;
    g_state.lines = 0;

    save_data data = ReadSaveData("data/data.txt");
    g_state.highScore = data.highScore;

    g_state.buttonPause = (button_t){
        .x      = 1770,
        .y      = 50,
        .width  = 100,
        .height = 100,
        .state  = button_state_idle
    };
}

static void CloseScene1(void) {
    EngineFree(g_data.tetrominoes[1].memory);
    EngineFree(g_data.tetrominoes[2].memory);
    EngineFree(g_data.tetrominoes[3].memory);
    EngineFree(g_data.tetrominoes[4].memory);
    EngineFree(g_data.tetrominoes[5].memory);
    EngineFree(g_data.tetrominoes[6].memory);
    EngineFree(g_data.tetrominoes[7].memory);

    EngineFree(g_data.tetrominoesUI[1].memory);
    EngineFree(g_data.tetrominoesUI[2].memory);
    EngineFree(g_data.tetrominoesUI[3].memory);
    EngineFree(g_data.tetrominoesUI[4].memory);
    EngineFree(g_data.tetrominoesUI[5].memory);
    EngineFree(g_data.tetrominoesUI[6].memory);
    EngineFree(g_data.tetrominoesUI[7].memory);

    EngineFree(g_data.background.memory);

    EngineFree(g_data.backgroundMusic.samples);
    EngineFree(g_data.sfxMove.samples);
    EngineFree(g_data.sfxRotate.samples);
    EngineFree(g_data.sfxLock.samples);
    EngineFree(g_data.sfxLineClear.samples);
    EngineFree(g_data.sfxHold.samples);
    EngineFree(g_data.sfxLevelUp.samples);
    EngineFree(g_data.sfxSoftDrop.samples);

    EngineFree(g_state.board.tiles);

    StopAllSounds(g_state.audioChannels, AUDIO_CHANNEL_COUNT);
}

static void Scene1(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    if (PRESSED(keyboardState->esc)) {
        InitScene3();
        g_state.currentScene = &Scene3;
        return;
    }

    if (keyboardState->right.isDown) {
        g_state.timerAutoMoveDelay += deltaTime;
        if (g_state.timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            g_state.timerAutoMove += deltaTime;
        }
        if (g_state.timerAutoMove >= AUTO_MOVE || keyboardState->right.didChangeState) {
            g_state.timerAutoMove = 0.0f;
            ++g_state.current.x;
            if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
                --g_state.current.x;
            }
            else {
                PlaySound(&g_data.sfxMove, false, SFX_MOVE, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }
    else if (keyboardState->left.isDown) {
        g_state.timerAutoMoveDelay += deltaTime;
        if (g_state.timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            g_state.timerAutoMove += deltaTime;
        }
        if (g_state.timerAutoMove >= AUTO_MOVE || keyboardState->left.didChangeState) {
            g_state.timerAutoMove = 0.0f;
            --g_state.current.x;
            if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
                ++g_state.current.x;
            }
            else {
                PlaySound(&g_data.sfxMove, false, SFX_MOVE, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }
    else {
        g_state.timerAutoMoveDelay = 0.0f;
    }

    i32 rotationDirection = PRESSED(keyboardState->x) - PRESSED(keyboardState->z);
    if (rotationDirection) {
        b32 didRotate = true;

        g_state.current.rotation = (g_state.current.rotation + rotationDirection + 4) % 4;
        if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
            g_state.current.x += 1;
            if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
                g_state.current.x -= 2;
                if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
                    g_state.current.x += 1;
                    g_state.current.y += 1;
                    if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
                        g_state.current.y -= 1;
                        g_state.current.rotation = (g_state.current.rotation - rotationDirection + 4) % 4;
                        didRotate = false;
                    }
                }
            }
        }

        if (didRotate) {
            PlaySound(&g_data.sfxRotate, false, SFX_ROTATE, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
        }
    }

    if (PRESSED(keyboardState->c) && !g_state.didUseHoldBox) {
        g_state.didUseHoldBox = true;

        tetromino_type currentType = g_state.current.type;
        if (g_state.hold.type == tetromino_type_empty) {
            g_state.current = InitTetromino(g_state.next[0].type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_data.tetrominoes);
            g_state.next[0].type = g_state.next[1].type;
            g_state.next[1].type = g_state.next[2].type;
            g_state.next[2].type = GetNextTetrominoFromBag(g_state.bag, &g_state.bagIndex);
        }
        else {
            g_state.current = InitTetromino(g_state.hold.type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_data.tetrominoes);
        }
        g_state.hold.type = currentType;

        PlaySound(&g_data.sfxHold, false, SFX_HOLD, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
    }

    b32 didSoftDrop = false;
    f32 gravityInSeconds = GetCurrentGravityInSeconds(g_state.level);
    if (keyboardState->down.isDown && gravityInSeconds > SOFT_DROP) {
        didSoftDrop = true;
        gravityInSeconds = SOFT_DROP;
    }

    b32 didHardDrop = false;
    if (PRESSED(keyboardState->up)) {
        didHardDrop = true;
        while (IsTetrominoPosValid(&g_state.board, &g_state.current)) {
            --g_state.current.y;
            g_state.score += SCORE_HARD_DROP * g_state.level;
        }
        ++g_state.current.y;
        g_state.score -= SCORE_HARD_DROP * g_state.level;
    }

    g_state.timerFall += deltaTime;
    if (g_state.timerFall >= gravityInSeconds || didHardDrop || g_state.timerLockDelay >= 0.001f) {
        g_state.timerFall = 0.0f;

        --g_state.current.y;

        if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
            ++g_state.current.y;

            g_state.timerLockDelay += deltaTime;
            if (g_state.timerLockDelay >= LOCK_DELAY || didHardDrop) {
                PlaceTetromino(&g_state.board, &g_state.current);

                i32 lineClearCount = ProcessLineClears(&g_state.board, &g_state.current);
                g_state.lines += lineClearCount;
                switch (lineClearCount) {
                    case 1: {
                        g_state.score += SCORE_SINGLE * g_state.level;
                    } break;
                    case 2: {
                        g_state.score += SCORE_DOUBLE * g_state.level;
                    } break;
                    case 3: {
                        g_state.score += SCORE_TRIPLE * g_state.level;
                    } break;
                    case 4: {
                        g_state.score += SCORE_QUADRUPLE * g_state.level;
                    } break;
                }


                if (g_state.lines >= g_state.level * 10) {
                    ++g_state.level;
                    PlaySound(&g_data.sfxLevelUp, false, SFX_LEVEL_UP, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
                }
                else {
                    switch (lineClearCount) {
                        case 1: {
                            PlaySound(&g_data.sfxLineClear, false, SFX_LINE_CLEAR, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 2: {
                            PlaySound(&g_data.sfxLineClear, false, SFX_LINE_CLEAR, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 3: {
                            PlaySound(&g_data.sfxLineClear, false, SFX_LINE_CLEAR, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 4: {
                            PlaySound(&g_data.sfxLineClear, false, SFX_LINE_CLEAR, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;

                        default: {
                            PlaySound(&g_data.sfxLock, false, SFX_LOCK, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                    }
                }

                g_state.current = InitTetromino(g_state.next[0].type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_data.tetrominoes);
                g_state.next[0].type = g_state.next[1].type;
                g_state.next[1].type = g_state.next[2].type;
                g_state.next[2].type = GetNextTetrominoFromBag(g_state.bag, &g_state.bagIndex);

                if (!IsTetrominoPosValid(&g_state.board, &g_state.current)) {
                    if (g_state.score > g_state.highScore) {
                        g_state.highScore = g_state.score;

                        save_data data = ReadSaveData("data/data.txt");
                        data.highScore = g_state.highScore;
                        EngineWriteEntireFile("data/data.txt", &data, sizeof(save_data));
                    }

                    CloseScene1();
                    InitScene2();
                    g_state.currentScene = &Scene2;
                    return;
                }

                g_state.timerAutoMoveDelay = 0.0f;
                g_state.timerLockDelay = 0.0f;
                g_state.didUseHoldBox = false;
            }
        }
        else {
            g_state.timerLockDelay = 0.0f;

            if (didSoftDrop) {
                g_state.score += SCORE_SOFT_DROP * g_state.level;
                PlaySound(&g_data.sfxSoftDrop, false, SFX_SOFT_DROP, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }

    tetromino_t ghost = g_state.current;
    while (IsTetrominoPosValid(&g_state.board, &ghost)) {
        --ghost.y;
    }
    ++ghost.y;

    Assert(g_data.background.width = graphicsBuffer->width);
    DrawBitmapStupid(graphicsBuffer, &g_data.background, 0, 0);

    DrawBoard(graphicsBuffer, &g_state.board, g_data.tetrominoes);

    DrawTetrominoInBoard(graphicsBuffer, &g_state.board, &g_state.current, &g_data.tetrominoes[g_state.current.type], 255);

    DrawTetrominoInBoard(graphicsBuffer, &g_state.board, &ghost, &g_data.tetrominoes[ghost.type], 64); // <-- Feedback :)

    // Could be replaced by DrawBitmapStupidWithOpacity for the sake of performance
    // The same goes for the rest of the calls to DrawBitmap that doesn't require scaling
    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.next[0].type], g_state.next[0].x, g_state.next[0].y, 90, 255);
    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.next[1].type], g_state.next[1].x, g_state.next[1].y, 90, 255);
    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.next[2].type], g_state.next[2].x, g_state.next[2].y, 90, 255);

    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.hold.type], g_state.hold.x, g_state.hold.y, 90, g_state.didUseHoldBox ? 128 : 255);

    DrawNumber(graphicsBuffer, &g_data.font, g_state.level, 578, 322, 3, true);
    DrawNumber(graphicsBuffer, &g_data.font, g_state.score, 578, 232, 3, true);
    DrawNumber(graphicsBuffer, &g_data.font, g_state.lines, 578, 142, 3, true);

    DrawNumber(graphicsBuffer, &g_data.font, g_state.highScore, 578, 457, 3, true);

    UpdateButtonState(&g_state.buttonPause, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    if (g_state.buttonPause.state == button_state_pressed) {
        InitScene3();
        g_state.currentScene = &Scene3;
    }

    u32 buttonColour = 0;
    switch (g_state.buttonPause.state) {
        case button_state_idle: {
            buttonColour = 0xFF0000;
        } break;
        case button_state_hover: {
            buttonColour = 0x00FF00;
        } break;
        case button_state_pressed: {
            buttonColour = 0x000000;
        } break;
        case button_state_held: {
            buttonColour = 0x0000FF;
        } break;
        case button_state_released: {
            buttonColour = 0xFFFFFF;
        } break;
    }
    DrawRectangle(graphicsBuffer, g_state.buttonPause.x, g_state.buttonPause.y, g_state.buttonPause.width, g_state.buttonPause.height, buttonColour);

    DrawText(graphicsBuffer, &g_data.font, "Hello, World", 10, 50, 3, false);
    DrawNumber(graphicsBuffer, &g_data.font, -12345, 10, 80, 3, false);

    extern u32 DEBUG_microsecondsElapsed;
    DrawNumber(graphicsBuffer, &g_data.font, DEBUG_microsecondsElapsed, 10, 4, 3, false);
}

// SCENE 2 //

static void InitScene2(void) {
    g_data.background = LoadBMP("assets/graphics/tetris_background_title2.bmp");

    g_data.buttonStartGame = LoadBMP("assets/graphics/start_game_button.bmp");
    g_data.buttonOptions = LoadBMP("assets/graphics/options_button.bmp");
    g_data.buttonQuitGame = LoadBMP("assets/graphics/quit_game_button.bmp");

    g_data.backgroundMusic = LoadWAV("assets/audio/Tetris3.wav");

    PlaySound(&g_data.backgroundMusic, true, BACKGROUND_MUSIC, g_state.audioChannels, AUDIO_CHANNEL_COUNT);

    g_state.buttonStart = (button_t){
        .x      = 830,
        .y      = 370,
        .width  = 260,
        .height = 90,
        .state  = button_state_idle
    };

    g_state.buttonOptions = (button_t){
        .x      = 860,
        .y      = 275,
        .width  = 200,
        .height = 90,
        .state  = button_state_idle
    };

    g_state.buttonQuit = (button_t){
        .x      = 840,
        .y      = 175,
        .width  = 240,
        .height = 90,
        .state  = button_state_idle
    };

    g_state.currentButtonIndex = 0;
}

static void CloseScene2(void) {
    EngineFree(g_data.background.memory);

    EngineFree(g_data.buttonStartGame.memory);
    EngineFree(g_data.buttonOptions.memory);
    EngineFree(g_data.buttonQuitGame.memory);

    EngineFree(g_data.backgroundMusic.samples);

    StopAllSounds(g_state.audioChannels, AUDIO_CHANNEL_COUNT);
}

// REFACTOR
// Also, ignore mouse cursor if keyboard was last used
static void Scene2(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    UpdateButtonState(&g_state.buttonStart,   keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&g_state.buttonOptions, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&g_state.buttonQuit,    keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    if (PRESSED(keyboardState->up) && g_state.currentButtonIndex != 0) {
        --g_state.currentButtonIndex;
    }
    else if (PRESSED(keyboardState->down) && g_state.currentButtonIndex != 2) {
        ++g_state.currentButtonIndex;
    }

    if (g_state.buttonStart.state == button_state_hover) {
        g_state.currentButtonIndex = 0;
    }
    else if (g_state.buttonOptions.state == button_state_hover) {
        g_state.currentButtonIndex = 1;
    }
    else if (g_state.buttonQuit.state == button_state_hover) {
        g_state.currentButtonIndex = 2;
    }

    if (g_state.buttonStart.state == button_state_pressed   || (g_state.currentButtonIndex == 0 && PRESSED(keyboardState->enter))) {
        CloseScene2();
        InitScene1();
        g_state.currentScene = &Scene1;
        return;
    }

    if (g_state.buttonOptions.state == button_state_pressed || (g_state.currentButtonIndex == 1 && PRESSED(keyboardState->enter))) {
        CloseScene2();
        InitScene4();
        g_state.currentScene = &Scene4;
        return;
    }

    if (g_state.buttonQuit.state == button_state_pressed    || (g_state.currentButtonIndex == 2 && PRESSED(keyboardState->enter))) {
        EngineClose();
        return;
    }

    DrawBitmapStupid(graphicsBuffer, &g_data.background, 0, 0);

    DrawBitmapStupidWithOpacity(graphicsBuffer, &g_data.buttonStartGame, 852, 400, 255);
    DrawBitmapStupidWithOpacity(graphicsBuffer, &g_data.buttonOptions, 884, 300, 255);
    DrawBitmapStupidWithOpacity(graphicsBuffer, &g_data.buttonQuitGame, 858, 200, 255);

    switch (g_state.currentButtonIndex) {
        case 0: {
            DrawRectangle(graphicsBuffer, 808, 409, 12, 12, 0xFFFFFF);
            DrawRectangle(graphicsBuffer, 820, 397, 12, 36, 0xFFFFFF);

            DrawRectangle(graphicsBuffer, 1100, 409, 12, 12, 0xFFFFFF);
            DrawRectangle(graphicsBuffer, 1088, 397, 12, 36, 0xFFFFFF);
        } break;
        case 1: {
            DrawRectangle(graphicsBuffer, 840, 314, 12, 12, 0xFFFFFF);
            DrawRectangle(graphicsBuffer, 852, 302, 12, 36, 0xFFFFFF);

            DrawRectangle(graphicsBuffer, 1068, 314, 12, 12, 0xFFFFFF);
            DrawRectangle(graphicsBuffer, 1056, 302, 12, 36, 0xFFFFFF);
        } break;
        case 2: {
            DrawRectangle(graphicsBuffer, 814, 212, 12, 12, 0xFFFFFF);
            DrawRectangle(graphicsBuffer, 826, 200, 12, 36, 0xFFFFFF);

            DrawRectangle(graphicsBuffer, 1094, 212, 12, 12, 0xFFFFFF);
            DrawRectangle(graphicsBuffer, 1082, 200, 12, 36, 0xFFFFFF);
        } break;
    }
}

// SCENE 3 //

static void InitScene3(void) {
    EngineFree(g_data.tetrominoes[1].memory);
    EngineFree(g_data.tetrominoes[2].memory);
    EngineFree(g_data.tetrominoes[3].memory);
    EngineFree(g_data.tetrominoes[4].memory);
    EngineFree(g_data.tetrominoes[5].memory);
    EngineFree(g_data.tetrominoes[6].memory);
    EngineFree(g_data.tetrominoes[7].memory);
    g_data.tetrominoes[1] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_i_dim.bmp");
    g_data.tetrominoes[2] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_o_dim.bmp");
    g_data.tetrominoes[3] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_t_dim.bmp");
    g_data.tetrominoes[4] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_s_dim.bmp");
    g_data.tetrominoes[5] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_z_dim.bmp");
    g_data.tetrominoes[6] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_j_dim.bmp");
    g_data.tetrominoes[7] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_l_dim.bmp");

    EngineFree(g_data.tetrominoesUI[1].memory);
    EngineFree(g_data.tetrominoesUI[2].memory);
    EngineFree(g_data.tetrominoesUI[3].memory);
    EngineFree(g_data.tetrominoesUI[4].memory);
    EngineFree(g_data.tetrominoesUI[5].memory);
    EngineFree(g_data.tetrominoesUI[6].memory);
    EngineFree(g_data.tetrominoesUI[7].memory);
    g_data.tetrominoesUI[1] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_i_ui_dim.bmp");
    g_data.tetrominoesUI[2] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_o_ui_dim.bmp");
    g_data.tetrominoesUI[3] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_t_ui_dim.bmp");
    g_data.tetrominoesUI[4] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_s_ui_dim.bmp");
    g_data.tetrominoesUI[5] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_z_ui_dim.bmp");
    g_data.tetrominoesUI[6] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_j_ui_dim.bmp");
    g_data.tetrominoesUI[7] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_l_ui_dim.bmp");

    EngineFree(g_data.background.memory);
    g_data.background = LoadBMP("assets/graphics/background_dim.bmp");

    CopyAudioChannels(g_state.tempAudioChannels, g_state.audioChannels, AUDIO_CHANNEL_COUNT);
    StopAllSounds(g_state.audioChannels, AUDIO_CHANNEL_COUNT);
}

static void CloseScene3(void) {
    EngineFree(g_data.tetrominoes[1].memory);
    EngineFree(g_data.tetrominoes[2].memory);
    EngineFree(g_data.tetrominoes[3].memory);
    EngineFree(g_data.tetrominoes[4].memory);
    EngineFree(g_data.tetrominoes[5].memory);
    EngineFree(g_data.tetrominoes[6].memory);
    EngineFree(g_data.tetrominoes[7].memory);
    g_data.tetrominoes[1] = LoadBMP("assets/graphics/tetrominoes/tetromino_i.bmp");
    g_data.tetrominoes[2] = LoadBMP("assets/graphics/tetrominoes/tetromino_o.bmp");
    g_data.tetrominoes[3] = LoadBMP("assets/graphics/tetrominoes/tetromino_t.bmp");
    g_data.tetrominoes[4] = LoadBMP("assets/graphics/tetrominoes/tetromino_s.bmp");
    g_data.tetrominoes[5] = LoadBMP("assets/graphics/tetrominoes/tetromino_z.bmp");
    g_data.tetrominoes[6] = LoadBMP("assets/graphics/tetrominoes/tetromino_j.bmp");
    g_data.tetrominoes[7] = LoadBMP("assets/graphics/tetrominoes/tetromino_l.bmp");

    EngineFree(g_data.tetrominoesUI[1].memory);
    EngineFree(g_data.tetrominoesUI[2].memory);
    EngineFree(g_data.tetrominoesUI[3].memory);
    EngineFree(g_data.tetrominoesUI[4].memory);
    EngineFree(g_data.tetrominoesUI[5].memory);
    EngineFree(g_data.tetrominoesUI[6].memory);
    EngineFree(g_data.tetrominoesUI[7].memory);
    g_data.tetrominoesUI[1] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_I_UI.bmp");
    g_data.tetrominoesUI[2] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_O_UI.bmp");
    g_data.tetrominoesUI[3] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_T_UI.bmp");
    g_data.tetrominoesUI[4] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_S_UI.bmp");
    g_data.tetrominoesUI[5] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_Z_UI.bmp");
    g_data.tetrominoesUI[6] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_J_UI.bmp");
    g_data.tetrominoesUI[7] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_L_UI.bmp");

    EngineFree(g_data.background.memory);
    g_data.background = LoadBMP("assets/graphics/background3.bmp");

    CopyAudioChannels(g_state.audioChannels, g_state.tempAudioChannels, AUDIO_CHANNEL_COUNT);
}

static void Scene3(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    UpdateButtonState(&g_state.buttonPause, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    u32 buttonColour = 0;
    switch (g_state.buttonPause.state) {
        case button_state_idle: {
            buttonColour = 0xFF0000;
        } break;
        case button_state_hover: {
            buttonColour = 0x00FF00;
        } break;
        case button_state_pressed: {
            buttonColour = 0x000000;
        } break;
        case button_state_held: {
            buttonColour = 0x0000FF;
        } break;
        case button_state_released: {
            buttonColour = 0xFFFFFF;
        } break;
    }

    tetromino_t ghost = g_state.current;
    while (IsTetrominoPosValid(&g_state.board, &ghost)) {
        --ghost.y;
    }
    ++ghost.y;

    DrawBitmapStupid(graphicsBuffer, &g_data.background, 0, 0);

    DrawBoard(graphicsBuffer, &g_state.board, g_data.tetrominoes);

    DrawTetrominoInBoard(graphicsBuffer, &g_state.board, &g_state.current, &g_data.tetrominoes[g_state.current.type], 255);

    DrawTetrominoInBoard(graphicsBuffer, &g_state.board, &ghost, &g_data.tetrominoes[ghost.type], 64);

    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.next[0].type], g_state.next[0].x, g_state.next[0].y, 90, 255);
    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.next[1].type], g_state.next[1].x, g_state.next[1].y, 90, 255);
    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.next[2].type], g_state.next[2].x, g_state.next[2].y, 90, 255);

    DrawBitmap(graphicsBuffer, &g_data.tetrominoesUI[g_state.hold.type], g_state.hold.x, g_state.hold.y, 90, g_state.didUseHoldBox ? 128 : 255);

    DrawNumber(graphicsBuffer, &g_data.font, g_state.level, 578, 322, 3, true);
    DrawNumber(graphicsBuffer, &g_data.font, g_state.score, 578, 232, 3, true);
    DrawNumber(graphicsBuffer, &g_data.font, g_state.lines, 578, 142, 3, true);

    DrawNumber(graphicsBuffer, &g_data.font, g_state.highScore, 578, 457, 3, true);

    DrawRectangle(graphicsBuffer, g_state.buttonPause.x, g_state.buttonPause.y, g_state.buttonPause.width, g_state.buttonPause.height, buttonColour);

    DrawText(graphicsBuffer, &g_data.font, "Paused", 960, 540, 3, true);

    if (PRESSED(keyboardState->esc) || g_state.buttonPause.state == button_state_pressed) {
        CloseScene3();
        g_state.currentScene = &Scene1;
        return;
    }
}

// Scene 4 //

static void InitScene4(void) { }

static void CloseScene4(void) { }

static void Scene4(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    DrawRectangle(graphicsBuffer, 0, 0, graphicsBuffer->width, graphicsBuffer->height, 0x000000);
    DrawText(graphicsBuffer, &g_data.font, "OPTIONS", 960, 520, 3, true);

    if (PRESSED(keyboardState->mouseLeft) || PRESSED(keyboardState->enter)) {
        CloseScene4();
        InitScene2();
        g_state.currentScene = &Scene2;
        return;
    }
}


void OnStartup(void) {
    g_data.font = InitFont("assets/graphics/letters_sprite_sheet2.bmp", 13, 5, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,.-");
    g_state.audioVolume = 1.0f;


    InitScene2();
    g_state.currentScene = &Scene2;
}

void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    if (PRESSED(keyboardState->f)) {
        EngineToggleFullscreen();
    }

    (*g_state.currentScene)(graphicsBuffer, soundBuffer, keyboardState, deltaTime);
    ProcessSound(soundBuffer, g_state.audioChannels, AUDIO_CHANNEL_COUNT, g_state.audioVolume);
}