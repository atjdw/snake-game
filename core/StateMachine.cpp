#include "../SnakeGame.h"

const char *getStateTextA(GameState state) {
    switch (state) {
        case STATE_START:
            return u8"开始";
        case STATE_RUNNING:
            return u8"进行中";
        case STATE_PAUSED:
            return u8"暂停";
        case STATE_OVER:
            return u8"结束";
        default:
            return u8"未知";
    }
}

const wchar_t *getStateTextW(GameState state) {
    switch (state) {
        case STATE_START:
            return L"开始";
        case STATE_RUNNING:
            return L"进行中";
        case STATE_PAUSED:
            return L"暂停";
        case STATE_OVER:
            return L"结束";
        default:
            return L"未知";
    }
}

int getDifficultyLevel(int speed) {
    if (speed >= 90) return 1;
    if (speed >= 75) return 2;
    if (speed >= 60) return 3;
    if (speed >= 45) return 4;
    return 5;
}

GameState nextState(GameState current, GameEvent event) {
    switch (current) {
        case STATE_START:
            if (event == EVENT_START_ACTION || event == EVENT_TOGGLE_PAUSE) return STATE_RUNNING;
            return STATE_START;
        case STATE_RUNNING:
            if (event == EVENT_TOGGLE_PAUSE) return STATE_PAUSED;
            if (event == EVENT_COLLISION) return STATE_OVER;
            return STATE_RUNNING;
        case STATE_PAUSED:
            if (event == EVENT_TOGGLE_PAUSE) return STATE_RUNNING;
            return STATE_PAUSED;
        case STATE_OVER:
            if (event == EVENT_LOAD_GAME) return STATE_PAUSED;
            return STATE_OVER;
        default:
            return STATE_START;
    }
}

void dispatchEvent(Game *game, GameEvent event) {
    if (event == EVENT_QUIT) {
        game->gameOver = 1;
        return;
    }

    game->state = nextState(game->state, event);
}

const char *getControllerTextA(ControllerMode mode) {
    return mode == CONTROLLER_AI ? u8"AI" : u8"玩家";
}

const wchar_t *getControllerTextW(ControllerMode mode) {
    return mode == CONTROLLER_AI ? L"AI" : L"玩家";
}
