#include "../SnakeGame.h"

#include <stdio.h>
#include <string.h>

int saveGame(const Game *game) {
    FILE *fp = fopen(SAVE_FILE, "w");
    if (!fp) return 0;

    fprintf(fp, "SNAKE_SAVE_V1\n");
    fprintf(fp,
            "%d %d %d %d %d %d %d %d %d %d\n",
            WIDTH,
            HEIGHT,
            (int)game->state,
            game->fullWidth,
            game->score,
            game->speed,
            game->snake.length,
            (int)game->snake.dir,
            game->food.x,
            game->food.y);

    for (int i = 0; i < game->snake.length; i++) {
        fprintf(fp, "%d %d\n", game->snake.body[i].x, game->snake.body[i].y);
    }

    fclose(fp);
    return 1;
}

int loadGame(Game *game) {
    FILE *fp = fopen(SAVE_FILE, "r");
    if (!fp) return 0;

    char header[64] = {0};
    if (!fgets(header, sizeof(header), fp)) {
        fclose(fp);
        return 0;
    }

    if (strncmp(header, "SNAKE_SAVE_V1", 13) != 0) {
        fclose(fp);
        return 0;
    }

    int saveWidth = 0, saveHeight = 0;
    int state = 0, fullWidth = 0, score = 0, speed = 0, length = 0, dir = 0;
    int foodX = 0, foodY = 0;

    if (fscanf(fp,
               "%d %d %d %d %d %d %d %d %d %d",
               &saveWidth,
               &saveHeight,
               &state,
               &fullWidth,
               &score,
               &speed,
               &length,
               &dir,
               &foodX,
               &foodY) != 10) {
        fclose(fp);
        return 0;
    }

    if (saveWidth != WIDTH || saveHeight != HEIGHT) {
        fclose(fp);
        return 0;
    }

    if (length < 1 || length > MAX_LENGTH || dir < UP || dir > STOP) {
        fclose(fp);
        return 0;
    }

    if (foodX < 0 || foodX >= WIDTH || foodY < 0 || foodY >= HEIGHT) {
        fclose(fp);
        return 0;
    }

    Snake loadedSnake;
    loadedSnake.length = length;
    loadedSnake.dir = (Direction)dir;

    for (int i = 0; i < length; i++) {
        int x = 0, y = 0;
        if (fscanf(fp, "%d %d", &x, &y) != 2) {
            fclose(fp);
            return 0;
        }

        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
            fclose(fp);
            return 0;
        }

        loadedSnake.body[i].x = x;
        loadedSnake.body[i].y = y;
    }

    fclose(fp);

    game->snake = loadedSnake;
    game->food.x = foodX;
    game->food.y = foodY;
    game->score = score;
    game->speed = speed;
    game->fullWidth = fullWidth ? 1 : 0;
    game->gameOver = 0;
    game->controller = CONTROLLER_HUMAN;
    game->desiredDir = game->snake.dir;

    if (state < STATE_START || state > STATE_OVER) {
        game->state = STATE_PAUSED;
    } else {
        game->state = (GameState)state;
        dispatchEvent(game, EVENT_LOAD_GAME);
    }

    if (game->speed < 20) game->speed = 20;
    if (game->speed > 300) game->speed = 300;

    return 1;
}
