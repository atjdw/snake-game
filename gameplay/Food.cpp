#include "../SnakeGame.h"

#include <stdlib.h>

void setupFood(Game *game) {
    int valid;
    do {
        valid = 1;
        game->food.x = rand() % (WIDTH - 2) + 1;
        game->food.y = rand() % (HEIGHT - 2) + 1;

        for (int i = 0; i < game->snake.length; i++) {
            if (game->food.x == game->snake.body[i].x && game->food.y == game->snake.body[i].y) {
                valid = 0;
                break;
            }
        }
    } while (!valid);
}
