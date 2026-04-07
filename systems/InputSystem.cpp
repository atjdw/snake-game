#include "../SnakeGame.h"

#include <conio.h>

int isOppositeDirection(Direction a, Direction b) {
    return (a == UP && b == DOWN) || (a == DOWN && b == UP) || (a == LEFT && b == RIGHT) || (a == RIGHT && b == LEFT);
}

int parseDirectionKey(int key, Direction *outDir) {
    switch (key) {
        case 'w':
        case 'W':
            *outDir = UP;
            return 1;
        case 's':
        case 'S':
            *outDir = DOWN;
            return 1;
        case 'a':
        case 'A':
            *outDir = LEFT;
            return 1;
        case 'd':
        case 'D':
            *outDir = RIGHT;
            return 1;
        default:
            return 0;
    }
}

void processInput(Game *game) {
    Direction pendingDir = game->snake.dir;
    int hasPendingDir = 0;

    while (_kbhit()) {
        int key = _getch();

        if (key == 0 || key == 224) {
            if (_kbhit()) _getch();
            if (game->state == STATE_START) dispatchEvent(game, EVENT_START_ACTION);
            continue;
        }

        if (key == '\r') {
            game->fullWidth = !game->fullWidth;
            if (game->state == STATE_START) dispatchEvent(game, EVENT_START_ACTION);
            continue;
        }

        if (key == 'k' || key == 'K') {
            saveGame(game);
            continue;
        }

        if (key == 'l' || key == 'L') {
            loadGame(game);
            continue;
        }

        if (key == 'p' || key == 'P') {
            dispatchEvent(game, EVENT_TOGGLE_PAUSE);
            continue;
        }

        if (key == 'm' || key == 'M') {
            game->controller = (game->controller == CONTROLLER_HUMAN) ? CONTROLLER_AI : CONTROLLER_HUMAN;
            game->desiredDir = game->snake.dir;
            continue;
        }

        if (key == 'x' || key == 'X') {
            dispatchEvent(game, EVENT_QUIT);
            return;
        }

        if (game->state == STATE_START) {
            dispatchEvent(game, EVENT_START_ACTION);
            continue;
        }

        if (game->state != STATE_RUNNING || game->controller != CONTROLLER_HUMAN) continue;

        Direction candidate;
        if (parseDirectionKey(key, &candidate)) {
            Direction baseDir = hasPendingDir ? pendingDir : game->snake.dir;
            if (!isOppositeDirection(baseDir, candidate)) {
                pendingDir = candidate;
                hasPendingDir = 1;
            }
        }
    }

    if (hasPendingDir) {
        game->desiredDir = pendingDir;
    }
}
