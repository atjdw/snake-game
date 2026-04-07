#include "../SnakeGame.h"

void runGameLoop(Game *game) {
    while (!game->gameOver) {
        processInput(game);
        if (game->state == STATE_RUNNING) {
            update(game);
        }
        render(game);
        sleepFrame(game->speed);
    }
}
