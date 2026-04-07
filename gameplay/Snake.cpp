#include "../SnakeGame.h"

#include <stdlib.h>

void initGame(Game *game) {
    game->snake.length = 3;
    game->snake.dir = RIGHT;

    int startX = WIDTH / 2;
    int startY = HEIGHT / 2;

    for (int i = 0; i < game->snake.length; i++) {
        game->snake.body[i].x = startX - i;
        game->snake.body[i].y = startY;
    }

    game->score = 0;
    game->gameOver = 0;
    game->speed = 100;
    game->state = STATE_START;
    game->fullWidth = 0;
    game->controller = CONTROLLER_HUMAN;
    game->desiredDir = game->snake.dir;

    setupFood(game);
}

static int isDirectionSafe(const Game *game, Direction dir) {
    Position next = game->snake.body[0];

    switch (dir) {
        case UP:
            next.y--;
            break;
        case DOWN:
            next.y++;
            break;
        case LEFT:
            next.x--;
            break;
        case RIGHT:
            next.x++;
            break;
        default:
            return 0;
    }

    if (next.x < 0 || next.x >= WIDTH || next.y < 0 || next.y >= HEIGHT) {
        return 0;
    }

    for (int i = 0; i < game->snake.length; i++) {
        if (next.x == game->snake.body[i].x && next.y == game->snake.body[i].y) {
            return 0;
        }
    }

    return 1;
}

static int simulateNextSnake(const Game *game, Direction dir, Snake *outSnake, Position *outHead, int *outAteFood) {
    if (dir == STOP) return 0;

    Position newHead = game->snake.body[0];
    switch (dir) {
        case UP:
            newHead.y--;
            break;
        case DOWN:
            newHead.y++;
            break;
        case LEFT:
            newHead.x--;
            break;
        case RIGHT:
            newHead.x++;
            break;
        default:
            return 0;
    }

    if (newHead.x < 0 || newHead.x >= WIDTH || newHead.y < 0 || newHead.y >= HEIGHT) {
        return 0;
    }

    for (int i = 1; i < game->snake.length; i++) {
        if (newHead.x == game->snake.body[i].x && newHead.y == game->snake.body[i].y) {
            return 0;
        }
    }

    Snake next = game->snake;
    int ateFood = (newHead.x == game->food.x && newHead.y == game->food.y);

    if (ateFood && next.length < MAX_LENGTH) {
        next.length++;
    }

    for (int i = next.length - 1; i > 0; i--) {
        next.body[i] = next.body[i - 1];
    }
    next.body[0] = newHead;
    next.dir = dir;

    *outSnake = next;
    *outHead = newHead;
    *outAteFood = ateFood;
    return 1;
}

static int shortestPathDistance(const Snake *snake, Position start, Position target) {
    if (start.x == target.x && start.y == target.y) return 0;

    int blocked[HEIGHT][WIDTH] = {{0}};
    int visited[HEIGHT][WIDTH] = {{0}};
    int dist[HEIGHT][WIDTH] = {{0}};
    Position queue[WIDTH * HEIGHT];
    int qh = 0, qt = 0;

    for (int i = 1; i < snake->length; i++) {
        int x = snake->body[i].x;
        int y = snake->body[i].y;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            blocked[y][x] = 1;
        }
    }

    queue[qt++] = start;
    visited[start.y][start.x] = 1;

    const int dx[4] = {0, 0, -1, 1};
    const int dy[4] = {-1, 1, 0, 0};

    while (qh < qt) {
        Position cur = queue[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dx[k];
            int ny = cur.y + dy[k];

            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
            if (visited[ny][nx] || blocked[ny][nx]) continue;

            visited[ny][nx] = 1;
            dist[ny][nx] = dist[cur.y][cur.x] + 1;

            if (nx == target.x && ny == target.y) {
                return dist[ny][nx];
            }

            queue[qt++] = (Position){nx, ny};
        }
    }

    return -1;
}

static int shortestPathDistanceTailAware(const Snake *snake, Position start, Position target, int tailFree) {
    if (start.x == target.x && start.y == target.y) return 0;

    int blocked[HEIGHT][WIDTH] = {{0}};
    int visited[HEIGHT][WIDTH] = {{0}};
    int dist[HEIGHT][WIDTH] = {{0}};
    Position queue[WIDTH * HEIGHT];
    int qh = 0, qt = 0;

    for (int i = 1; i < snake->length; i++) {
        if (tailFree && i == snake->length - 1) continue;

        int x = snake->body[i].x;
        int y = snake->body[i].y;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            blocked[y][x] = 1;
        }
    }

    queue[qt++] = start;
    visited[start.y][start.x] = 1;

    const int dx[4] = {0, 0, -1, 1};
    const int dy[4] = {-1, 1, 0, 0};

    while (qh < qt) {
        Position cur = queue[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dx[k];
            int ny = cur.y + dy[k];

            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
            if (visited[ny][nx] || blocked[ny][nx]) continue;

            visited[ny][nx] = 1;
            dist[ny][nx] = dist[cur.y][cur.x] + 1;

            if (nx == target.x && ny == target.y) {
                return dist[ny][nx];
            }

            queue[qt++] = (Position){nx, ny};
        }
    }

    return -1;
}

static int reachableArea(const Snake *snake, Position start) {
    int blocked[HEIGHT][WIDTH] = {{0}};
    int visited[HEIGHT][WIDTH] = {{0}};
    Position queue[WIDTH * HEIGHT];
    int qh = 0, qt = 0;
    int area = 0;

    for (int i = 1; i < snake->length; i++) {
        int x = snake->body[i].x;
        int y = snake->body[i].y;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            blocked[y][x] = 1;
        }
    }

    if (blocked[start.y][start.x]) return 0;

    queue[qt++] = start;
    visited[start.y][start.x] = 1;

    const int dx[4] = {0, 0, -1, 1};
    const int dy[4] = {-1, 1, 0, 0};

    while (qh < qt) {
        Position cur = queue[qh++];
        area++;

        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dx[k];
            int ny = cur.y + dy[k];

            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
            if (visited[ny][nx] || blocked[ny][nx]) continue;

            visited[ny][nx] = 1;
            queue[qt++] = (Position){nx, ny};
        }
    }

    return area;
}

static int manhattanDistance(Position a, Position b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

Direction chooseAIDirection(const Game *game) {
    Direction dirs[4] = {UP, DOWN, LEFT, RIGHT};
    Direction bestDir = game->snake.dir;
    int bestScore = -1000000000;
    int bestTier = -1;

    for (int i = 0; i < 4; i++) {
        Direction candidate = dirs[i];
        if (isOppositeDirection(game->snake.dir, candidate)) continue;
        if (!isDirectionSafe(game, candidate)) continue;

        Snake nextSnake;
        Position nextHead;
        int ateFood = 0;
        if (!simulateNextSnake(game, candidate, &nextSnake, &nextHead, &ateFood)) continue;

        int distToFood = shortestPathDistance(&nextSnake, nextHead, game->food);
        Position tail = nextSnake.body[nextSnake.length - 1];
        int distToTail = shortestPathDistanceTailAware(&nextSnake, nextHead, tail, 1);
        int area = reachableArea(&nextSnake, nextHead);
        int safetyNeed = nextSnake.length + 2;

        int score = 0;
        int tier = 0;

        if (distToFood >= 0 && distToTail >= 0 && area >= safetyNeed) {
            tier = 2;
        } else if (distToTail >= 0) {
            tier = 1;
        }

        if (distToFood >= 0) score += 6000 - distToFood * 22;
        else score -= manhattanDistance(nextHead, game->food) * 6;

        if (distToTail >= 0) score += 2200 - distToTail * 10;
        else score -= 4500;

        score += area * 12;

        if (area < safetyNeed) {
            score -= 5000;
        }

        if (ateFood) {
            score += 120;
        }

        if (candidate == game->snake.dir) {
            score += 20;
        }

        if (tier > bestTier || (tier == bestTier && score > bestScore)) {
            bestTier = tier;
            bestScore = score;
            bestDir = candidate;
        }
    }

    return bestDir;
}

void applyControllerDecision(Game *game) {
    if (game->controller == CONTROLLER_AI) {
        game->desiredDir = chooseAIDirection(game);
    }

    if (!isOppositeDirection(game->snake.dir, game->desiredDir)) {
        game->snake.dir = game->desiredDir;
    }
}

void update(Game *game) {
    applyControllerDecision(game);

    Position newHead = game->snake.body[0];

    switch (game->snake.dir) {
        case UP:
            newHead.y--;
            break;
        case DOWN:
            newHead.y++;
            break;
        case LEFT:
            newHead.x--;
            break;
        case RIGHT:
            newHead.x++;
            break;
        case STOP:
            return;
    }

    if (newHead.x < 0 || newHead.x >= WIDTH || newHead.y < 0 || newHead.y >= HEIGHT) {
        dispatchEvent(game, EVENT_COLLISION);
        return;
    }

    for (int i = 1; i < game->snake.length; i++) {
        if (newHead.x == game->snake.body[i].x && newHead.y == game->snake.body[i].y) {
            dispatchEvent(game, EVENT_COLLISION);
            return;
        }
    }

    int ateFood = (newHead.x == game->food.x && newHead.y == game->food.y);

    if (!ateFood) {
        for (int i = game->snake.length - 1; i > 0; i--) {
            game->snake.body[i] = game->snake.body[i - 1];
        }
    } else {
        if (game->snake.length < MAX_LENGTH) {
            game->snake.length++;
            for (int i = game->snake.length - 1; i > 0; i--) {
                game->snake.body[i] = game->snake.body[i - 1];
            }
            game->score += 10;
            if (game->speed > 30) game->speed -= 2;
            setupFood(game);
        }
    }

    game->snake.body[0] = newHead;
}
