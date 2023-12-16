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

#define BACKGROUND_MUSIC 0.75f
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
#define SCORE_TETRIS    1200
#define SCORE_SOFT_DROP 1
#define SCORE_HARD_DROP 2

#define SAVE_DATA_PATH "data/data.txt"


#define PRESSED(key) (key.isDown && key.didChangeState)


typedef enum tetromino_type {
    tetromino_type_empty = 0,
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

typedef struct tetromino_t {
    tetromino_type type;
    i32 rotation;
    i32 x;
    i32 y;
} tetromino_t;

typedef struct board_t {
    tetromino_type* tiles;
    i32 width;
    i32 height;
    i32 size;     // ?
    i32 x;
    i32 y;
    i32 tileSize;
    i32 widthPx;  // ?
    i32 heightPx; // ?
} board_t;

typedef enum button_state {
    button_state_idle = 0,
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

typedef struct save_data {
    i32 highScore;
    f32 masterVolume;
    f32 soundVolume;
    f32 musicVolume;
} save_data;

typedef struct global_state {
    scene_pointer currentScene;
    audio_channel audioChannels[AUDIO_CHANNEL_COUNT];

    save_data saveData;
} global_state;

typedef struct global_data {
    font_t font;
} global_data;


static global_state g_globalState;
static global_data  g_globalData;
static void* g_sceneState;
static void* g_sceneData;


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

static void ResetSaveData(save_data* data) {
    *data = (save_data){
        .highScore = 0,
        .masterVolume = 1.0f,
        .soundVolume = 1.0f,
        .musicVolume = 1.0f
    };
}

static save_data ReadSaveData(const char* filePath) {
    i32 bytesRead = 0;
    void* contents = EngineReadEntireFile(filePath, &bytesRead);

    save_data data;
    if (bytesRead != sizeof(save_data)) {
        ResetSaveData(&data);
        EngineWriteEntireFile(filePath, &data, sizeof(save_data));
    }
    else {
        data = *(save_data*)contents;
    }
    
    EngineFree(contents);

    return data;
}

static void WriteSaveData(const char* filePath, save_data* data) {
    EngineWriteEntireFile(filePath, data, sizeof(save_data));
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


// SCENE 1: Gameplay //

typedef struct scene1_state {
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
} scene1_state;

typedef struct scene1_data {
    bitmap_buffer tetrominoes[8];
    bitmap_buffer tetrominoesUI[8];

    bitmap_buffer background;

    bitmap_buffer buttonPauseUnpaused;

    sound_buffer backgroundMusic;
    sound_buffer sfxMove;
    sound_buffer sfxRotate;
    sound_buffer sfxLock;
    sound_buffer sfxLineClear;
    sound_buffer sfxHold;
    sound_buffer sfxLevelUp;
    sound_buffer sfxSoftDrop;
} scene1_data;

static void InitScene1(void) {
    g_sceneState = EngineAllocate(sizeof(scene1_state));
    g_sceneData  = EngineAllocate(sizeof(scene1_data));

    scene1_state* state = g_sceneState;
    scene1_data*  data  = g_sceneData;


    data->tetrominoes[1] = LoadBMP("assets/graphics/tetrominoes/tetromino_i.bmp");
    data->tetrominoes[2] = LoadBMP("assets/graphics/tetrominoes/tetromino_o.bmp");
    data->tetrominoes[3] = LoadBMP("assets/graphics/tetrominoes/tetromino_t.bmp");
    data->tetrominoes[4] = LoadBMP("assets/graphics/tetrominoes/tetromino_s.bmp");
    data->tetrominoes[5] = LoadBMP("assets/graphics/tetrominoes/tetromino_z.bmp");
    data->tetrominoes[6] = LoadBMP("assets/graphics/tetrominoes/tetromino_j.bmp");
    data->tetrominoes[7] = LoadBMP("assets/graphics/tetrominoes/tetromino_l.bmp");

    data->tetrominoesUI[1] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_I_UI.bmp");
    data->tetrominoesUI[2] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_O_UI.bmp");
    data->tetrominoesUI[3] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_T_UI.bmp");
    data->tetrominoesUI[4] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_S_UI.bmp");
    data->tetrominoesUI[5] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_Z_UI.bmp");
    data->tetrominoesUI[6] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_J_UI.bmp");
    data->tetrominoesUI[7] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_L_UI.bmp");

    data->background = LoadBMP("assets/graphics/background3.bmp");

    data->buttonPauseUnpaused = LoadBMP("assets/graphics/button_pause_unpaused.bmp");

    data->backgroundMusic = LoadWAV("assets/audio/tetris_theme.wav");

    // Look these over
    data->sfxMove      = LoadWAV("assets/audio/sfx1.wav");
    data->sfxRotate    = LoadWAV("assets/audio/sfx4.wav");
    data->sfxLock      = LoadWAV("assets/audio/sfx3.wav");
    data->sfxLineClear = LoadWAV("assets/audio/sfx5.wav"); 
    data->sfxHold      = LoadWAV("assets/audio/sfx2.wav");
    data->sfxLevelUp   = LoadWAV("assets/audio/sfx6.wav");
    data->sfxSoftDrop  = LoadWAV("assets/audio/sfx1.wav");

    StopAllSounds(g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
    PlaySound(&data->backgroundMusic, true, BACKGROUND_MUSIC * g_globalState.saveData.musicVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);

    RandomInit();

    state->board = InitBoard(BOARD_WIDTH, BOARD_HEIGHT, 735, 90, 45);

    RandomizeBag(state->bag);

    state->current = InitTetromino(GetNextTetrominoFromBag(state->bag, &state->bagIndex), 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4);
    state->next[0] = InitTetromino(GetNextTetrominoFromBag(state->bag, &state->bagIndex), 0, 1298, 788);
    state->next[1] = InitTetromino(GetNextTetrominoFromBag(state->bag, &state->bagIndex), 0, 1298, 653);
    state->next[2] = InitTetromino(GetNextTetrominoFromBag(state->bag, &state->bagIndex), 0, 1298, 518);
    state->hold = InitTetromino(tetromino_type_empty, 0, 533, 788);

    state->score = 0;
    state->level = 1;
    state->lines = 0;

    save_data saveData = ReadSaveData(SAVE_DATA_PATH);
    g_globalState.saveData.highScore = saveData.highScore;

    state->buttonPause = (button_t){
        .x      = 1770,
        .y      = 50,
        .width  = 80,
        .height = 80,
        .state  = button_state_idle
    };
}

static void CloseScene1(void) {
    scene1_state* state = g_sceneState;
    scene1_data*  data  = g_sceneData;


    EngineFree(data->tetrominoes[1].memory);
    EngineFree(data->tetrominoes[2].memory);
    EngineFree(data->tetrominoes[3].memory);
    EngineFree(data->tetrominoes[4].memory);
    EngineFree(data->tetrominoes[5].memory);
    EngineFree(data->tetrominoes[6].memory);
    EngineFree(data->tetrominoes[7].memory);

    EngineFree(data->tetrominoesUI[1].memory);
    EngineFree(data->tetrominoesUI[2].memory);
    EngineFree(data->tetrominoesUI[3].memory);
    EngineFree(data->tetrominoesUI[4].memory);
    EngineFree(data->tetrominoesUI[5].memory);
    EngineFree(data->tetrominoesUI[6].memory);
    EngineFree(data->tetrominoesUI[7].memory);

    EngineFree(data->background.memory);

    EngineFree(data->buttonPauseUnpaused.memory);

    EngineFree(data->backgroundMusic.samples);
    EngineFree(data->sfxMove.samples);
    EngineFree(data->sfxRotate.samples);
    EngineFree(data->sfxLock.samples);
    EngineFree(data->sfxLineClear.samples);
    EngineFree(data->sfxHold.samples);
    EngineFree(data->sfxLevelUp.samples);
    EngineFree(data->sfxSoftDrop.samples);

    EngineFree(state->board.tiles);


    EngineFree(g_sceneState);
    EngineFree(g_sceneData);
    g_sceneState = 0;
    g_sceneData  = 0;
}

static void Scene1(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    scene1_state* state = g_sceneState;
    scene1_data*  data  = g_sceneData;


    UpdateButtonState(&state->buttonPause, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    // This is not a good solution
    if (PRESSED(keyboardState->esc) || state->buttonPause.state == button_state_pressed) {
        InitScene3();
        g_globalState.currentScene = &Scene3;

        // This is so scuffed lol. Would this thing even work on another computer?
        *(scene1_state**)g_sceneState = state;
        *(scene1_data**)g_sceneData   = data;

        return;
    }

    if (keyboardState->right.isDown) {
        state->timerAutoMoveDelay += deltaTime;
        if (state->timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            state->timerAutoMove += deltaTime;
        }
        if (state->timerAutoMove >= AUTO_MOVE || keyboardState->right.didChangeState) {
            state->timerAutoMove = 0.0f;
            ++state->current.x;
            if (!IsTetrominoPosValid(&state->board, &state->current)) {
                --state->current.x;
            }
            else {
                PlaySound(&data->sfxMove, false, SFX_MOVE * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }
    else if (keyboardState->left.isDown) {
        state->timerAutoMoveDelay += deltaTime;
        if (state->timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            state->timerAutoMove += deltaTime;
        }
        if (state->timerAutoMove >= AUTO_MOVE || keyboardState->left.didChangeState) {
            state->timerAutoMove = 0.0f;
            --state->current.x;
            if (!IsTetrominoPosValid(&state->board, &state->current)) {
                ++state->current.x;
            }
            else {
                PlaySound(&data->sfxMove, false, SFX_MOVE * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }
    else {
        state->timerAutoMoveDelay = 0.0f;
    }

    i32 rotationDirection = PRESSED(keyboardState->x) - PRESSED(keyboardState->z);
    if (rotationDirection) {
        b32 didRotate = true;

        state->current.rotation = (state->current.rotation + rotationDirection + 4) % 4;
        if (!IsTetrominoPosValid(&state->board, &state->current)) {
            state->current.x += 1;
            if (!IsTetrominoPosValid(&state->board, &state->current)) {
                state->current.x -= 2;
                if (!IsTetrominoPosValid(&state->board, &state->current)) {
                    state->current.x += 1;
                    state->current.y += 1;
                    if (!IsTetrominoPosValid(&state->board, &state->current)) {
                        state->current.y -= 1;
                        state->current.rotation = (state->current.rotation - rotationDirection + 4) % 4;
                        didRotate = false;
                    }
                }
            }
        }

        if (didRotate) {
            PlaySound(&data->sfxRotate, false, SFX_ROTATE * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
        }
    }

    if (PRESSED(keyboardState->c) && !state->didUseHoldBox) {
        state->didUseHoldBox = true;

        tetromino_type currentType = state->current.type;
        if (state->hold.type == tetromino_type_empty) {
            state->current = InitTetromino(state->next[0].type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &data->tetrominoes);
            state->next[0].type = state->next[1].type;
            state->next[1].type = state->next[2].type;
            state->next[2].type = GetNextTetrominoFromBag(state->bag, &state->bagIndex);
        }
        else {
            state->current = InitTetromino(state->hold.type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &data->tetrominoes);
        }
        state->hold.type = currentType;

        PlaySound(&data->sfxHold, false, SFX_HOLD * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
    }

    b32 didSoftDrop = false;
    f32 gravityInSeconds = GetCurrentGravityInSeconds(state->level);
    if (keyboardState->down.isDown && gravityInSeconds > SOFT_DROP) {
        didSoftDrop = true;
        gravityInSeconds = SOFT_DROP;
    }

    b32 didHardDrop = false;
    if (PRESSED(keyboardState->up)) {
        didHardDrop = true;
        while (IsTetrominoPosValid(&state->board, &state->current)) {
            --state->current.y;
            state->score += SCORE_HARD_DROP * state->level;
        }
        ++state->current.y;
        state->score -= SCORE_HARD_DROP * state->level;
    }

    state->timerFall += deltaTime;
    if (state->timerFall >= gravityInSeconds || didHardDrop || state->timerLockDelay >= 0.0001f) {
        state->timerFall = 0.0f;

        --state->current.y;

        if (!IsTetrominoPosValid(&state->board, &state->current)) {
            ++state->current.y;

            state->timerLockDelay += deltaTime;
            if (state->timerLockDelay >= LOCK_DELAY || didHardDrop) {
                PlaceTetromino(&state->board, &state->current);

                i32 lineClearCount = ProcessLineClears(&state->board, &state->current);
                state->lines += lineClearCount;
                switch (lineClearCount) {
                    case 1: {
                        state->score += SCORE_SINGLE * state->level;
                    } break;
                    case 2: {
                        state->score += SCORE_DOUBLE * state->level;
                    } break;
                    case 3: {
                        state->score += SCORE_TRIPLE * state->level;
                    } break;
                    case 4: {
                        state->score += SCORE_TETRIS * state->level;
                    } break;
                }


                if (state->lines >= state->level * 10) {
                    ++state->level;
                    PlaySound(&data->sfxLevelUp, false, SFX_LEVEL_UP * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
                }
                else {
                    switch (lineClearCount) {
                        case 1: {
                            PlaySound(&data->sfxLineClear, false, SFX_LINE_CLEAR * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 2: {
                            PlaySound(&data->sfxLineClear, false, SFX_LINE_CLEAR * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 3: {
                            PlaySound(&data->sfxLineClear, false, SFX_LINE_CLEAR * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 4: {
                            PlaySound(&data->sfxLineClear, false, SFX_LINE_CLEAR * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;

                        default: {
                            PlaySound(&data->sfxLock, false, SFX_LOCK * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                    }
                }

                state->current = InitTetromino(state->next[0].type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &data->tetrominoes);
                state->next[0].type = state->next[1].type;
                state->next[1].type = state->next[2].type;
                state->next[2].type = GetNextTetrominoFromBag(state->bag, &state->bagIndex);

                if (!IsTetrominoPosValid(&state->board, &state->current)) {
                    if (state->score > g_globalState.saveData.highScore) {
                        g_globalState.saveData.highScore = state->score;

                        save_data saveData = ReadSaveData(SAVE_DATA_PATH);
                        saveData.highScore = g_globalState.saveData.highScore;
                        WriteSaveData(SAVE_DATA_PATH, &saveData);
                    }

                    CloseScene1();
                    InitScene2();
                    g_globalState.currentScene = &Scene2;
                    return;
                }

                state->timerAutoMoveDelay = 0.0f;
                state->timerLockDelay = 0.0f;
                state->didUseHoldBox = false;
            }
        }
        else {
            state->timerLockDelay = 0.0f;

            if (didSoftDrop) {
                state->score += SCORE_SOFT_DROP * state->level;
                PlaySound(&data->sfxSoftDrop, false, SFX_SOFT_DROP * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }

    tetromino_t ghost = state->current;
    while (IsTetrominoPosValid(&state->board, &ghost)) {
        --ghost.y;
    }
    ++ghost.y;

    DrawBitmapStupid(graphicsBuffer, &data->background, 0, 0);

    DrawBoard(graphicsBuffer, &state->board, data->tetrominoes);

    DrawTetrominoInBoard(graphicsBuffer, &state->board, &state->current, &data->tetrominoes[state->current.type], 255);

    DrawTetrominoInBoard(graphicsBuffer, &state->board, &ghost, &data->tetrominoes[ghost.type], 64); // <-- Feedback :)

    // Could be replaced by DrawBitmapStupidWithOpacity for the sake of performance
    // The same goes for the rest of the calls to DrawBitmap that doesn't require scaling
    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->next[0].type], state->next[0].x, state->next[0].y, 90, 255);
    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->next[1].type], state->next[1].x, state->next[1].y, 90, 255);
    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->next[2].type], state->next[2].x, state->next[2].y, 90, 255);

    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->hold.type], state->hold.x, state->hold.y, 90, state->didUseHoldBox ? 128 : 255);

    DrawNumber(graphicsBuffer, &g_globalData.font, state->level, 578, 322, 3, true);
    DrawNumber(graphicsBuffer, &g_globalData.font, state->score, 578, 232, 3, true);
    DrawNumber(graphicsBuffer, &g_globalData.font, state->lines, 578, 142, 3, true);

    DrawNumber(graphicsBuffer, &g_globalData.font, g_globalState.saveData.highScore, 578, 457, 3, true);

    // Redo graphic
    DrawBitmap(graphicsBuffer, &data->buttonPauseUnpaused, state->buttonPause.x, state->buttonPause.y, state->buttonPause.width, 255);
}

// SCENE 2: Main menu //

typedef struct scene2_state {
    button_t buttonStart;
    button_t buttonOptions;
    button_t buttonQuit;
    i32 currentButtonIndex;
} scene2_state;

typedef struct scene2_data {
    bitmap_buffer background;

    bitmap_buffer buttonStart;
    bitmap_buffer buttonOptions;
    bitmap_buffer buttonQuit;

    sound_buffer backgroundMusic;

    sound_buffer sfxButtonSwitch;
} scene2_data;

static void InitScene2(void) {
    g_sceneState = EngineAllocate(sizeof(scene2_state));
    g_sceneData  = EngineAllocate(sizeof(scene2_data));

    scene2_state* state = g_sceneState;
    scene2_data*  data  = g_sceneData;


    data->background = LoadBMP("assets/graphics/tetris_background_title2.bmp");

    data->buttonStart   = LoadBMP("assets/graphics/start_game_button.bmp");
    data->buttonOptions = LoadBMP("assets/graphics/options_button.bmp");
    data->buttonQuit    = LoadBMP("assets/graphics/quit_game_button.bmp");

    data->backgroundMusic = LoadWAV("assets/audio/tetris_theme.wav");

    data->sfxButtonSwitch = LoadWAV("assets/audio/sfx1.wav");

    StopAllSounds(g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
    PlaySound(&data->backgroundMusic, true, BACKGROUND_MUSIC * g_globalState.saveData.musicVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);

    state->buttonStart = (button_t){
        .x      = 830,
        .y      = 370,
        .width  = 260,
        .height = 90,
        .state  = button_state_idle
    };

    state->buttonOptions = (button_t){
        .x      = 860,
        .y      = 275,
        .width  = 200,
        .height = 90,
        .state  = button_state_idle
    };

    state->buttonQuit = (button_t){
        .x      = 840,
        .y      = 175,
        .width  = 240,
        .height = 90,
        .state  = button_state_idle
    };

    state->currentButtonIndex = 0;
}

static void CloseScene2(void) {
    scene2_state* state = g_sceneState;
    scene2_data*  data  = g_sceneData;


    EngineFree(data->background.memory);

    EngineFree(data->buttonStart.memory);
    EngineFree(data->buttonOptions.memory);
    EngineFree(data->buttonQuit.memory);

    EngineFree(data->backgroundMusic.samples);

    EngineFree(data->sfxButtonSwitch.samples);


    EngineFree(g_sceneState);
    EngineFree(g_sceneData);
    g_sceneState = 0;
    g_sceneData  = 0;
}

static void Scene2(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    scene2_state* state = g_sceneState;
    scene2_data*  data  = g_sceneData;


    UpdateButtonState(&state->buttonStart,   keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->buttonOptions, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->buttonQuit,    keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    i32 initialButtonIndex = state->currentButtonIndex;

    if (state->buttonStart.state == button_state_hover) {
        state->currentButtonIndex = 0;
    }
    else if (state->buttonOptions.state == button_state_hover) {
        state->currentButtonIndex = 1;
    }
    else if (state->buttonQuit.state == button_state_hover) {
        state->currentButtonIndex = 2;
    }

    if (PRESSED(keyboardState->up) && state->currentButtonIndex != 0) {
        --state->currentButtonIndex;
    }
    else if (PRESSED(keyboardState->down) && state->currentButtonIndex != 2) {
        ++state->currentButtonIndex;
    }

    if (state->buttonStart.state == button_state_pressed || (state->currentButtonIndex == 0 && PRESSED(keyboardState->enter))) {
        CloseScene2();
        InitScene1();
        g_globalState.currentScene = &Scene1;
        return;
    }

    if (state->buttonOptions.state == button_state_pressed || (state->currentButtonIndex == 1 && PRESSED(keyboardState->enter))) {
        CloseScene2();
        InitScene4();
        g_globalState.currentScene = &Scene4;
        return;
    }

    if (state->buttonQuit.state == button_state_pressed || (state->currentButtonIndex == 2 && PRESSED(keyboardState->enter))) {
        EngineClose();
        return;
    }

    if (state->currentButtonIndex != initialButtonIndex) {
        PlaySound(&data->sfxButtonSwitch, false, SFX_MOVE * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
    }

    DrawBitmapStupid(graphicsBuffer, &data->background, 0, 0);

    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->buttonStart,   852, 400, 255);
    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->buttonOptions, 884, 300, 255);
    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->buttonQuit,    858, 200, 255);

    // Hardcoding values :)
    i32 markerXLeft = 0;
    i32 markerXRight = 0;
    i32 markerY = 0;
    const i32 markerSize = 12;
    const u32 markerColour = 0xFFFFFF;
    switch (state->currentButtonIndex) {
        case 0: {
            markerXLeft = 808;
            markerXRight = 1088;
            markerY = 397;
        } break;
        case 1: {
            markerXLeft = 840;
            markerXRight = 1056;
            markerY = 302;
        } break;
        case 2: {
            markerXLeft = 814;
            markerXRight = 1082;
            markerY = 200;
        } break;
    }
    DrawRectangle(graphicsBuffer, markerXLeft, markerY + markerSize, markerSize, markerSize, markerColour);
    DrawRectangle(graphicsBuffer, markerXLeft + markerSize, markerY, markerSize, 3 * markerSize, markerColour);
    DrawRectangle(graphicsBuffer, markerXRight + markerSize, markerY + markerSize, markerSize, markerSize, markerColour);
    DrawRectangle(graphicsBuffer, markerXRight, markerY, markerSize, 3 * markerSize, markerColour);
}

// SCENE 3: Paused //

typedef struct scene3_state {
    scene1_state* scene1;
} scene3_state;

typedef struct scene3_data {
    scene1_data* scene1;

    audio_channel tempAudioChannels[AUDIO_CHANNEL_COUNT];

    bitmap_buffer tetrominoes[8];
    bitmap_buffer tetrominoesUI[8];

    bitmap_buffer background;

    bitmap_buffer buttonPausePaused;
} scene3_data;

static void InitScene3(void) {
    g_sceneState = EngineAllocate(sizeof(scene3_state));
    g_sceneData  = EngineAllocate(sizeof(scene3_data));

    scene3_state* state = g_sceneState;
    scene3_data*  data  = g_sceneData;


    data->tetrominoes[1] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_i_dim.bmp");
    data->tetrominoes[2] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_o_dim.bmp");
    data->tetrominoes[3] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_t_dim.bmp");
    data->tetrominoes[4] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_s_dim.bmp");
    data->tetrominoes[5] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_z_dim.bmp");
    data->tetrominoes[6] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_j_dim.bmp");
    data->tetrominoes[7] = LoadBMP("assets/graphics/tetrominoes/dim/tetromino_l_dim.bmp");

    data->tetrominoesUI[1] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_i_ui_dim.bmp");
    data->tetrominoesUI[2] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_o_ui_dim.bmp");
    data->tetrominoesUI[3] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_t_ui_dim.bmp");
    data->tetrominoesUI[4] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_s_ui_dim.bmp");
    data->tetrominoesUI[5] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_z_ui_dim.bmp");
    data->tetrominoesUI[6] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_j_ui_dim.bmp");
    data->tetrominoesUI[7] = LoadBMP("assets/graphics/tetrominoes_ui/dim/tetromino_l_ui_dim.bmp");

    data->background = LoadBMP("assets/graphics/background_dim.bmp");

    data->buttonPausePaused = LoadBMP("assets/graphics/button_pause_paused.bmp");

    CopyAudioChannels(data->tempAudioChannels, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
    StopAllSounds(g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
}

static void CloseScene3(void) {
    scene3_state* state = g_sceneState;
    scene3_data*  data  = g_sceneData;


    EngineFree(data->tetrominoes[1].memory);
    EngineFree(data->tetrominoes[2].memory);
    EngineFree(data->tetrominoes[3].memory);
    EngineFree(data->tetrominoes[4].memory);
    EngineFree(data->tetrominoes[5].memory);
    EngineFree(data->tetrominoes[6].memory);
    EngineFree(data->tetrominoes[7].memory);

    EngineFree(data->tetrominoesUI[1].memory);
    EngineFree(data->tetrominoesUI[2].memory);
    EngineFree(data->tetrominoesUI[3].memory);
    EngineFree(data->tetrominoesUI[4].memory);
    EngineFree(data->tetrominoesUI[5].memory);
    EngineFree(data->tetrominoesUI[6].memory);
    EngineFree(data->tetrominoesUI[7].memory);

    EngineFree(data->background.memory);

    EngineFree(data->buttonPausePaused.memory);


    EngineFree(g_sceneState);
    EngineFree(g_sceneData);
    g_sceneState = 0;
    g_sceneData  = 0;
}

static void Scene3(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    scene3_state* state = g_sceneState;
    scene3_data*  data  = g_sceneData;


    UpdateButtonState(&state->scene1->buttonPause, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    tetromino_t ghost = state->scene1->current;
    while (IsTetrominoPosValid(&state->scene1->board, &ghost)) {
        --ghost.y;
    }
    ++ghost.y;

    DrawBitmapStupid(graphicsBuffer, &data->background, 0, 0);

    DrawBoard(graphicsBuffer, &state->scene1->board, data->tetrominoes);

    DrawTetrominoInBoard(graphicsBuffer, &state->scene1->board, &state->scene1->current, &data->tetrominoes[state->scene1->current.type], 255);

    DrawTetrominoInBoard(graphicsBuffer, &state->scene1->board, &ghost, &data->tetrominoes[ghost.type], 64);

    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->scene1->next[0].type], state->scene1->next[0].x, state->scene1->next[0].y, 90, 255);
    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->scene1->next[1].type], state->scene1->next[1].x, state->scene1->next[1].y, 90, 255);
    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->scene1->next[2].type], state->scene1->next[2].x, state->scene1->next[2].y, 90, 255);

    DrawBitmap(graphicsBuffer, &data->tetrominoesUI[state->scene1->hold.type], state->scene1->hold.x, state->scene1->hold.y, 90, state->scene1->didUseHoldBox ? 128 : 255);

    DrawNumber(graphicsBuffer, &g_globalData.font, state->scene1->level, 578, 322, 3, true);
    DrawNumber(graphicsBuffer, &g_globalData.font, state->scene1->score, 578, 232, 3, true);
    DrawNumber(graphicsBuffer, &g_globalData.font, state->scene1->lines, 578, 142, 3, true);

    DrawNumber(graphicsBuffer, &g_globalData.font, g_globalState.saveData.highScore, 578, 457, 3, true);

    DrawBitmap(graphicsBuffer, &data->buttonPausePaused, state->scene1->buttonPause.x, state->scene1->buttonPause.y, state->scene1->buttonPause.width, 255);

    DrawText(graphicsBuffer, &g_globalData.font, "Paused", 960, 540, 3, true);

    // Assumes scene 1 was never closed
    if (PRESSED(keyboardState->esc) || state->scene1->buttonPause.state == button_state_pressed) {
        scene1_state* tempState = state->scene1;
        scene1_data* tempData = data->scene1;

        CopyAudioChannels(g_globalState.audioChannels, data->tempAudioChannels, AUDIO_CHANNEL_COUNT);

        CloseScene3();
        g_globalState.currentScene = &Scene1;

        g_sceneState = tempState;
        g_sceneData  = tempData;

        return;
    }
}

// Scene 4: Options //

typedef struct scene4_state {
    i32 masterVolume;
    i32 soundVolume;
    i32 musicVolume;

    button_t sliderMasterVolume;
    button_t sliderSoundVolume;
    button_t sliderMusicVolume;
    button_t labelMasterVolume;
    button_t labelSoundVolume;
    button_t labelMusicVolume;
    button_t buttonResetHighscore;
    button_t buttonBack;

    i32 currentSelectedIndex;
} scene4_state;

typedef struct scene4_data {
    bitmap_buffer background;

    bitmap_buffer buttonBack;
    bitmap_buffer buttonResetHighcore;
    bitmap_buffer labelMasterVolume;
    bitmap_buffer labelSoundVolume;
    bitmap_buffer labelMusicVolume;

    sound_buffer backgroundMusic;

    sound_buffer sfxButtonSwitch;
} scene4_data;

static void InitScene4(void) {
    g_sceneState = EngineAllocate(sizeof(scene4_state));
    g_sceneData  = EngineAllocate(sizeof(scene4_data));

    scene4_state* state = g_sceneState;
    scene4_data*  data  = g_sceneData;


    state->masterVolume = g_globalState.saveData.masterVolume * 10;
    state->soundVolume  = g_globalState.saveData.soundVolume  * 10;
    state->musicVolume  = g_globalState.saveData.musicVolume  * 10;

    state->sliderMasterVolume = (button_t){
        .x      = 1090 + 10 * state->masterVolume,
        .y      = 652,
        .width  = 20,
        .height = 20,
        .state  = button_state_idle
    };

    state->sliderSoundVolume = (button_t){
        .x      = 1090 + 10 * state->soundVolume,
        .y      = 562,
        .width  = 20,
        .height = 20,
        .state  = button_state_idle
    };

    state->sliderMusicVolume = (button_t){
        .x      = 1090 + 10 * state->musicVolume,
        .y      = 472,
        .width  = 20,
        .height = 20,
        .state  = button_state_idle
    };

    state->labelMasterVolume = (button_t){
        .x      = 520,
        .y      = 625,
        .width  = 820,
        .height = 75,
        .state  = button_state_idle
    };

    state->labelSoundVolume = (button_t){
        .x      = 520,
        .y      = 535,
        .width  = 820,
        .height = 75,
        .state  = button_state_idle
    };

    state->labelMusicVolume = (button_t){
        .x      = 520,
        .y      = 445,
        .width  = 820,
        .height = 75,
        .state  = button_state_idle
    };

    state->buttonResetHighscore = (button_t){
        .x      = 770,
        .y      = 225,
        .width  = 380,
        .height = 90,
        .state  = button_state_idle
    };

    state->buttonBack = (button_t){
        .x      = 880,
        .y      = 88,
        .width  = 160,
        .height = 90,
        .state  = button_state_idle
    };

    state->currentSelectedIndex = 4;

    data->background = LoadBMP("assets/graphics/background_options.bmp");

    data->labelMasterVolume = LoadBMP("assets/graphics/master_volume_label.bmp");
    data->labelSoundVolume  = LoadBMP("assets/graphics/sound_volume_label.bmp");
    data->labelMusicVolume  = LoadBMP("assets/graphics/music_volume_label.bmp");

    data->buttonResetHighcore = LoadBMP("assets/graphics/reset_highscore_button.bmp");

    data->buttonBack = LoadBMP("assets/graphics/back_button.bmp");

    data->backgroundMusic = LoadWAV("assets/audio/tetris_theme.wav");

    data->sfxButtonSwitch = LoadWAV("assets/audio/sfx1.wav");

    StopAllSounds(g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);

    PlaySound(&data->backgroundMusic, true, BACKGROUND_MUSIC * g_globalState.saveData.musicVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
}

static void CloseScene4(void) {
    scene4_state* state = g_sceneState;
    scene4_data*  data  = g_sceneData;


    EngineFree(data->background.memory);

    EngineFree(data->labelMasterVolume.memory);
    EngineFree(data->labelSoundVolume.memory);
    EngineFree(data->labelMusicVolume.memory);

    EngineFree(data->buttonResetHighcore.memory);

    EngineFree(data->buttonBack.memory);

    EngineFree(data->backgroundMusic.samples);

    EngineFree(data->sfxButtonSwitch.samples);


    WriteSaveData(SAVE_DATA_PATH, &g_globalState.saveData);


    EngineFree(g_sceneState);
    EngineFree(g_sceneData);
    g_sceneState = 0;
    g_sceneData  = 0;
}

// Add drop shadow to text?
// Full screen button
// Better indicators (like main men scene)

static void Scene4(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    scene4_state* state = g_sceneState;
    scene4_data*  data  = g_sceneData;

    if (state->sliderMasterVolume.state == button_state_held) {
        state->sliderMasterVolume.x = keyboardState->mouseX - state->sliderMasterVolume.width / 2;
        state->sliderMasterVolume.x = Clamp(state->sliderMasterVolume.x, 1090, 1090 + 200);

        state->masterVolume = (state->sliderMasterVolume.x - 1090) / 10;
        g_globalState.saveData.masterVolume = state->masterVolume / 10.0f;
    }
    else if (state->sliderSoundVolume.state == button_state_held) {
        state->sliderSoundVolume.x = keyboardState->mouseX - state->sliderSoundVolume.width / 2;
        state->sliderSoundVolume.x = Clamp(state->sliderSoundVolume.x, 1090, 1090 + 200);

        state->soundVolume = (state->sliderSoundVolume.x - 1090) / 10;
        g_globalState.saveData.soundVolume = state->soundVolume / 10.0f;
    }
    else if (state->sliderMusicVolume.state == button_state_held) {
        state->sliderMusicVolume.x = keyboardState->mouseX - state->sliderMusicVolume.width / 2;
        state->sliderMusicVolume.x = Clamp(state->sliderMusicVolume.x, 1090, 1090 + 200);

        state->musicVolume = (state->sliderMusicVolume.x - 1090) / 10;
        g_globalState.saveData.musicVolume = state->musicVolume / 10.0f;

        g_globalState.audioChannels[0].volume = g_globalState.saveData.musicVolume; // Awful solution. Replace!
    }

    i32 initialButtonIndex = state->currentSelectedIndex;

    UpdateButtonState(&state->sliderMasterVolume, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->sliderSoundVolume, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->sliderMusicVolume, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->labelMasterVolume, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->labelSoundVolume, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->labelMusicVolume, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->buttonResetHighscore, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);
    UpdateButtonState(&state->buttonBack, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    if (keyboardState->mouseLeft.isDown && !keyboardState->mouseLeft.didChangeState) {
        if (state->sliderMasterVolume.state == button_state_released) {
            state->sliderMasterVolume.state = button_state_held;
        }
        else if (state->sliderSoundVolume.state == button_state_released) {
            state->sliderSoundVolume.state = button_state_held;
        }
        else if (state->sliderMusicVolume.state == button_state_released) {
            state->sliderMusicVolume.state = button_state_held;
        }
    }

    if (state->labelMasterVolume.state == button_state_hover) {
        state->currentSelectedIndex = 0;
    }
    else if (state->labelSoundVolume.state == button_state_hover) {
        state->currentSelectedIndex = 1;
    }
    else if (state->labelMusicVolume.state == button_state_hover) {
        state->currentSelectedIndex = 2;
    }
    else if (state->buttonResetHighscore.state == button_state_hover) {
        state->currentSelectedIndex = 3;
    }
    else if (state->buttonBack.state == button_state_hover) {
        state->currentSelectedIndex = 4;
    }

    if (PRESSED(keyboardState->up) && state->currentSelectedIndex != 0) {
        --state->currentSelectedIndex;
    }
    else if (PRESSED(keyboardState->down) && state->currentSelectedIndex != 4) {
        ++state->currentSelectedIndex;
    }

    if (state->buttonResetHighscore.state == button_state_pressed || (state->currentSelectedIndex == 3 && PRESSED(keyboardState->enter))) {
        g_globalState.saveData.highScore = 0;
    }

    if (state->buttonBack.state == button_state_pressed || (state->currentSelectedIndex == 4 && PRESSED(keyboardState->enter))) {
        CloseScene4();
        InitScene2();
        g_globalState.currentScene = &Scene2;
        return;
    }

    switch (state->currentSelectedIndex) {
        case 0: {
            state->masterVolume += PRESSED(keyboardState->right) - PRESSED(keyboardState->left);
            state->masterVolume = Clamp(state->masterVolume, 0, 20);
            g_globalState.saveData.masterVolume = state->masterVolume / 10.0f;

            state->sliderMasterVolume.x = 1090 + 10 * state->masterVolume;
        } break;
        case 1: {
            state->soundVolume += PRESSED(keyboardState->right) - PRESSED(keyboardState->left);
            state->soundVolume = Clamp(state->soundVolume, 0, 20);
            g_globalState.saveData.soundVolume = state->soundVolume / 10.0f;

            state->sliderSoundVolume.x = 1090 + 10 * state->soundVolume;
        } break;
        case 2: {
            state->musicVolume += PRESSED(keyboardState->right) - PRESSED(keyboardState->left);
            state->musicVolume = Clamp(state->musicVolume, 0, 20);
            g_globalState.saveData.musicVolume = state->musicVolume / 10.0f;

            state->sliderMusicVolume.x = 1090 + 10 * state->musicVolume;

            g_globalState.audioChannels[0].volume = g_globalState.saveData.musicVolume; // Awful solution. Replace!
        } break;
    }

    if (initialButtonIndex != state->currentSelectedIndex) {
        PlaySound(&data->sfxButtonSwitch, false, BACKGROUND_MUSIC * g_globalState.saveData.soundVolume, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT);
    }

    // Graphics

    DrawBitmapStupid(graphicsBuffer, &data->background, 0, 0);

    i32 markerXLeft = 0;
    i32 markerXRight = 0;
    i32 markerY = 0;
    const i32 markerSize = 12;
    const u32 markerColour = 0xFFFFFF;
    switch (state->currentSelectedIndex) {
        case 0: {
            markerXLeft = 505;
            markerXRight = 1340;
            markerY = 645;
        } break;
        case 1: {
            markerXLeft = 505;
            markerXRight = 1340;
            markerY = 555;
        } break;
        case 2: {
            markerXLeft = 505;
            markerXRight = 1340;
            markerY = 465;
        } break;
        case 3: {
            markerXLeft = 755;
            markerXRight = 1140;
            markerY = 252;
        } break;
        case 4: {
            markerXLeft = 865;
            markerXRight = 1030;
            markerY = 115;
        } break;
    }
    DrawRectangle(graphicsBuffer, markerXLeft, markerY + markerSize, markerSize, markerSize, markerColour);
    DrawRectangle(graphicsBuffer, markerXLeft + markerSize, markerY, markerSize, 3 * markerSize, markerColour);
    DrawRectangle(graphicsBuffer, markerXRight + markerSize, markerY + markerSize, markerSize, markerSize, markerColour);
    DrawRectangle(graphicsBuffer, markerXRight, markerY, markerSize, 3 * markerSize, markerColour);

    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->labelMasterVolume, 560, 650, 255);
    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->labelSoundVolume, 560, 560, 255);
    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->labelMusicVolume, 560, 470, 255);

    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->buttonResetHighcore, 806, 250, 255);
    DrawBitmapStupidWithOpacity(graphicsBuffer, &data->buttonBack, 914, 120, 255);

    DrawRectangle(graphicsBuffer, 1100, 660, state->sliderMasterVolume.x - 1100, 3, 0xFFFFFF);
    DrawRectangle(graphicsBuffer, 1100, 570, state->sliderSoundVolume.x  - 1100, 3, 0xFFFFFF);
    DrawRectangle(graphicsBuffer, 1100, 480, state->sliderMusicVolume.x  - 1100, 3, 0xFFFFFF);
    DrawRectangle(graphicsBuffer, state->sliderMasterVolume.x, 660, 1300 - state->sliderMasterVolume.x, 3, 0x888888);
    DrawRectangle(graphicsBuffer, state->sliderSoundVolume.x,  570, 1300 - state->sliderSoundVolume.x,  3, 0x888888);
    DrawRectangle(graphicsBuffer, state->sliderMusicVolume.x,  480, 1300 - state->sliderMusicVolume.x,  3, 0x888888);

    DrawRectangle(graphicsBuffer, state->sliderMasterVolume.x, state->sliderMasterVolume.y, state->sliderMasterVolume.width, state->sliderMasterVolume.height, 0xFFFFFF);
    DrawRectangle(graphicsBuffer, state->sliderSoundVolume.x, state->sliderSoundVolume.y, state->sliderSoundVolume.width, state->sliderSoundVolume.height, 0xFFFFFF);
    DrawRectangle(graphicsBuffer, state->sliderMusicVolume.x, state->sliderMusicVolume.y, state->sliderMusicVolume.width, state->sliderMusicVolume.height, 0xFFFFFF);
}


void OnStartup(void) {
    g_globalData.font = InitFont("assets/graphics/letters_sprite_sheet2.bmp", 13, 5, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,.-");

    g_globalState.saveData = ReadSaveData(SAVE_DATA_PATH);


    InitScene2();
    g_globalState.currentScene = &Scene2;
}

// Rename graphicsBuffer to backBuffer please
void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    if (PRESSED(keyboardState->f)) {
        EngineToggleFullscreen();
    }

    if (keyboardState->didMouseMove) {
        keyboardState->isMouseVisible = true;
    }
    for (i32 i = 0; i < ArraySize(keyboardState->keys); ++i) {
        if (PRESSED(keyboardState->keys[i])) {
            keyboardState->isMouseVisible = false;
            break;
        }
    }

    (*g_globalState.currentScene)(graphicsBuffer, soundBuffer, keyboardState, deltaTime);
    ProcessSound(soundBuffer, g_globalState.audioChannels, AUDIO_CHANNEL_COUNT, g_globalState.saveData.masterVolume);
}