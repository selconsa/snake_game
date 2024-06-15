#include "SnakeGame.h"
#include "MapInit.h"
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <ncurses.h>

SnakeGame::SnakeGame(int width, int height, int level)
    : width(width), height(height), level(level), direction(-1), lastGateChangeTime(0), maxlength(13), growthItemCount(0), poisonItemCount(0), gateCount(0), scoreBoard(width), isPoison(false), poisonLastAppeared(0), reverseControlItemExists(false), reverseControlItemLastAppeared(0), reverseControlActive(false), reverseControlActivatedAt(0), growthLastAppeared(0) {
    initscr();
    cbreak();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    timeout(100);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK); //벽 색
        init_pair(2, COLOR_BLUE, COLOR_BLACK);  // 게이트 파란색
        init_pair(3, COLOR_RED, COLOR_BLACK);   // 길이 늘어가는거 빨간색
        init_pair(4, COLOR_GREEN, COLOR_BLACK); // 줄어드는거 초록색
        init_pair(5, COLOR_YELLOW, COLOR_BLACK); // 반대방향 노란색
    }

    map.resize(height, std::vector<int>(width, 0));
    MapInit::initMap(level, width, height, map, gate1, gate2);
    initializeSnake();
    placeGrowth(); //증가 아이템 위치
    srand(time(0)); // 랜덤 시간
    lastGateChangeTime = time(NULL);
    poisonLastAppeared = time(NULL);
    reverseControlItemLastAppeared = time(NULL);
    growthLastAppeared = time(NULL);
}

SnakeGame::~SnakeGame() {
    endwin();
}

void SnakeGame::run() {
    while (true) {
        clear();
        drawMap();
        drawSnake();
        drawGrowth();
        drawPoison();
        drawReverseControlItem();
        scoreBoard.draw();
        refresh();

        int ch = getch();
        if (ch == 'q') break;

        if (ch != ERR) {
            changeDirection(ch);
        }

        if (direction != -1) {
            moveSnake();
        }

        if (checkCollision()) {
            mvprintw(height / 2, (width - 10) / 2, "Game Over");
            refresh();
            sleep(3);
            resetGame();
            continue;
        }

        if (checkClearCondition()) {
            mvprintw(height / 2, (width - 10) / 2, "Stage Clear!");
            refresh();
            sleep(2);
            nextLevel();
            continue;
        }

        // 게이트 20초
        if (difftime(time(NULL), lastGateChangeTime) >= 20) {
            changeGatePosition();
            lastGateChangeTime = time(NULL);
        }

        // 증가 아이템 5초
        if (difftime(time(NULL), growthLastAppeared) >= 5) {
            placeGrowth();
            growthLastAppeared = time(NULL);
        }

        // 감소 아이템 5초 생김, 10초 안생김
        if (isPoison && difftime(time(NULL), poisonLastAppeared) >= 5) {
            isPoison = false;
            poisonLastAppeared = time(NULL);
        } else if (!isPoison && difftime(time(NULL), poisonLastAppeared) >= 10) {
            isPoison = true;
            placePoison();
            poisonLastAppeared = time(NULL);
        }

        // 반대방향 5초 생김 15초 안생김
        if (reverseControlItemExists && difftime(time(NULL), reverseControlItemLastAppeared) >= 5) {
            reverseControlItemExists = false;
            reverseControlItemLastAppeared = time(NULL);
        } else if (!reverseControlItemExists && difftime(time(NULL), reverseControlItemLastAppeared) >= 15) {
            reverseControlItemExists = true;
            placeReverseControlItem();
            reverseControlItemLastAppeared = time(NULL);
        }

        // 반대방향 효과
        if (reverseControlActive && difftime(time(NULL), reverseControlActivatedAt) >= 5) {
            reverseControlActive = false;
        }

        usleep(100000);//0.1 간격으로 뱀 움직임
    }
}

void SnakeGame::initializeSnake() {
    snake.clear();
    snake.push_back({1, 3}); // 머리
    snake.push_back({1, 2});
    snake.push_back({1, 1});
}

void SnakeGame::drawMap() {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int displayX = 2 * x; // x 좌표를 두 배로 해서 비율 맞춤
            if (map[y][x] == 2) {
                attron(COLOR_PAIR(1));
                mvprintw(y, displayX, "#");
                attroff(COLOR_PAIR(1));
            } else if (map[y][x] == 1) {
                attron(COLOR_PAIR(1));
                mvprintw(y, displayX, "+");
                attroff(COLOR_PAIR(1));
            } else if (map[y][x] == 3) {
                attron(COLOR_PAIR(2));
                mvprintw(y, displayX, "G"); // 문 표시
                attroff(COLOR_PAIR(2));
            }
        }
    }
}

void SnakeGame::drawSnake() {
    for (size_t i = 0; i < snake.size(); ++i) {
        int y = snake[i].first;
        int x = 2 * snake[i].second; // x 좌표를 두 배로 해서 비율 맞춤
        if (i == 0) {
            mvprintw(y, x, "O");
        } else {
            mvprintw(y, x, "o");
        }
    }
}

void SnakeGame::drawGrowth() {
    attron(COLOR_PAIR(3));
    mvprintw(growthItem.first, 2 * growthItem.second, "G");
    attroff(COLOR_PAIR(3));
}

void SnakeGame::drawPoison() {
    if (isPoison) {
        attron(COLOR_PAIR(4));
        mvprintw(poisonItem.first, 2 * poisonItem.second, "P");
        attroff(COLOR_PAIR(4));
    }
}

void SnakeGame::drawReverseControlItem() {
    if (reverseControlItemExists) {
        attron(COLOR_PAIR(5));
        mvprintw(reverseControlItem.first, 2 * reverseControlItem.second, "R");
        attroff(COLOR_PAIR(5));
    }
}

void SnakeGame::changeDirection(int ch) {
    if (reverseControlActive) {
        switch (ch) {
            case KEY_UP:
            case 'w':
                if (direction != 2) direction = 2;
                break;
            case KEY_RIGHT:
            case 'd':
                if (direction != 3) direction = 3;
                break;
            case KEY_DOWN:
            case 's':
                if (direction != 0) direction = 0;
                break;
            case KEY_LEFT:
            case 'a':
                if (direction != 1) direction = 1;
                break;
        }
    } else {
        switch (ch) {
            case KEY_UP:
            case 'w':
                if (direction != 2) direction = 0;
                break;
            case KEY_RIGHT:
            case 'd':
                if (direction != 3) direction = 1;
                break;
            case KEY_DOWN:
            case 's':
                if (direction != 0) direction = 2;
                break;
            case KEY_LEFT:
            case 'a':
                if (direction != 1) direction = 3;
                break;
        }
    }
}

void SnakeGame::moveSnake() {
    std::pair<int, int> newHead = snake.front();
    switch (direction) {
        case 0: newHead.first--; break; // Up
        case 1: newHead.second++; break; // Right
        case 2: newHead.first++; break; // Down
        case 3: newHead.second--; break; // Left
    }

    if (newHead != std::make_pair(1, 3)) {
        scoreBoard.startPlayTime();
    }

    // 텔레포트 후에도 안전한지 검증
    if (newHead == gate1) {
        teleport(newHead, gate2);
        gateCount++; 
    } else if (newHead == gate2) {
        teleport(newHead, gate1);
        gateCount++; 
    }

    // 텔레포트 후에 다시 위치 검증
    if (map[newHead.first][newHead.second] == 2 || map[newHead.first][newHead.second] == 1) {
        mvprintw(height / 2, (width - 10) / 2, "Game Over");
        refresh();
        sleep(3);
        resetGame();
        return;
    }

    // 증가 아이템 먹었을 때 몸길이 증가
    if (newHead == growthItem) {
        if (snake.size() < maxlength) {
            snake.insert(snake.begin(), newHead); 
        }
        placeGrowth(); // 증가 아이템 위치
        growthItemCount++;
        growthLastAppeared = time(NULL); 
        scoreBoard.update(growthItemCount, snake.size(), poisonItemCount, gateCount);
    } else if (isPoison && newHead == poisonItem) {
        if (snake.size() > 3) {
            snake.pop_back(); 
            drawSnake(); 
            refresh();
        } else {
            snake.pop_back(); 
            drawSnake(); 
            refresh();
            scoreBoard.update(growthItemCount, snake.size(), poisonItemCount, gateCount);
            mvprintw(height / 2, (width - 10) / 2, "Game Over");
            refresh();
            sleep(3);
            resetGame();
            return;
        }
        isPoison = false; // 감소아이템 제거
        poisonLastAppeared = time(NULL); // 감소 아이템이 사라진 시간을 설정
        poisonItemCount++; 
    } else if (reverseControlItemExists && newHead == reverseControlItem) {
        reverseControlActive = true; // 방향 바꾸기
        reverseControlActivatedAt = time(NULL); // 방향 바꾸기 시작시간
        reverseControlItemExists = false; // 아이템 제거
        reverseControlItemLastAppeared = time(NULL); // 감소 아이템 사라진 시간
    } else {
        snake.pop_back(); // 몸길이 13 도달했을 때
        snake.insert(snake.begin(), newHead);
    }

    scoreBoard.update(growthItemCount, snake.size(), poisonItemCount, gateCount);
}

void SnakeGame::teleport(std::pair<int, int>& head, const std::pair<int, int>& target) {
    // 이전 위치와 방향을 저장
    int prevDirection = direction;

    head = target; // 새로운 위치로 이동

    // 1. 나오는 포탈이 21*21 맵의 가장자리에 위치한 포탈이라면 방향과 상관없이 사각형 안쪽 방향, 벽과 수직으로 나오게 한다.
    if (isEdgeGate(head)) {
        if (head.first == 0) {
            direction = 2; head.first++;
        } else if (head.first == height - 1) {
            direction = 0; head.first--;
        } else if (head.second == 0) {
            direction = 1; head.second++;
        } else if (head.second == width - 1) {
            direction = 3; head.second--;
        }
    }
    // 2. 나오는 포탈이 가장자리에 위치하지 않으면, 들어간 방향으로 나온다.
    else {
        direction = prevDirection;

        // 3. 나오는 방향 바로 앞에 벽이 있으면, 들어간 방향에서 90도 시계방향 회전해서 나온다.
        switch (direction) {
            case 0: // Up
                if (map[head.first - 1][head.second] == 2 || map[head.first - 1][head.second] == 1) direction = 1; // Right
                break;
            case 1: // Right
                if (map[head.first][head.second + 1] == 2 || map[head.first][head.second + 1] == 1) direction = 2; // Down
                break;
            case 2: // Down
                if (map[head.first + 1][head.second] == 2 || map[head.first + 1][head.second] == 1) direction = 3; // Left
                break;
            case 3: // Left
                if (map[head.first][head.second - 1] == 2 || map[head.first][head.second - 1] == 1) direction = 0; // Up
                break;
        }
    }
}

bool SnakeGame::isEdgeGate(const std::pair<int, int>& pos) {
    return pos.first == 0 || pos.first == height - 1 || pos.second == 0 || pos.second == width - 1;
}

bool SnakeGame::checkCollision() {
    const std::pair<int, int>& head = snake.front();
    if (map[head.first][head.second] == 2 || map[head.first][head.second] == 1) {
        return true;
    }
    for (size_t i = 1; i < snake.size(); ++i) {
        if (snake[i] == head) return true;
    }
    return false;
}

void SnakeGame::resetGame() {
    //버퍼 비우기
    flushinp();

    direction = -1;
    initializeSnake();
    clear();
    MapInit::initMap(level, width, height, map, gate1, gate2);
    placeGrowth();
    growthItemCount = 0;
    poisonItemCount = 0;
    gateCount = 0;
    scoreBoard.reset();
    drawMap();
    drawSnake();
    drawGrowth();
    scoreBoard.draw();
    refresh();
}

void SnakeGame::placeGrowth() {
    int x, y;
    do {
        x = rand() % width;
        y = rand() % height;
    } while (map[y][x] != 0 || isSnakePosition(y, x)); //아무것도 없는 위치

    growthItem = {y, x};
}

void SnakeGame::placePoison() {
    int x, y;
    do {
        x = rand() % width;
        y = rand() % height;
    } while (map[y][x] != 0 || isSnakePosition(y, x) || growthItem == std::make_pair(y, x)); 

    poisonItem = {y, x};
}

void SnakeGame::placeReverseControlItem() {
    int x, y;
    do {
        x = rand() % width;
        y = rand() % height;
    } while (map[y][x] != 0 || isSnakePosition(y, x) || growthItem == std::make_pair(y, x));

    reverseControlItem = {y, x};
}

bool SnakeGame::isSnakePosition(int y, int x) {
    for (const auto& part : snake) {
        if (part.first == y && part.second == x) {
            return true;
        }
    }
    return false;
}

void SnakeGame::changeGatePosition() {
    MapInit::randomGates(width, height, map, gate1, gate2);
}

bool SnakeGame::checkClearCondition() {
    return (snake.size() >= 13 && growthItemCount >= 8 && poisonItemCount >= 3 && gateCount >= 5);
}

void SnakeGame::nextLevel() {
    level++;
    if (level > 4) level = 1;
    resetGame();
    clear();
    mvprintw(height / 2, (width - 10) / 2, "Next Level: %d", level);
    refresh();
    sleep(2);
    clear();
    MapInit::initMap(level, width, height, map, gate1, gate2);
    initializeSnake();
    placeGrowth();
    growthItemCount = 0;
    poisonItemCount = 0;
    gateCount = 0;
    scoreBoard.reset();
    drawMap();
    drawSnake();
    drawGrowth();
    scoreBoard.draw();
    refresh();
}
