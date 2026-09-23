/*
 * LA LINEA MINI GAMES
 * ESP32 Dev Module
 *
 * Hardware:
 * OLED SDA = GPIO4
 * OLED SCL = GPIO15
 * OLED RESET = GPIO16
 * Joystick VRx = GPIO32
 * Joystick VRy = GPIO33
 * Joystick SW  = GPIO25
 *
 * V3 foundation:
 *   1. Dino Jump
 *   2. Box Climber
 *   3. Free Walk
 *   4. Ping Pong
 *   5. Snake
 *   6. Breakout
 *   7. Space Invaders
 *   8. Asteroids
 *   9. Flappy
 *   10. Racing
 *   11. Memory
 *   12. Coin Collector
 *
 * V3 adds:
 *   - Scrollable game menu
 *   - Explicit function prototypes
 *   - RAM high-score framework
 *   - Non-blocking frame timing foundation
 *   - First additional games: Snake + Breakout
 *
 * Future versions will add more games, Wi-Fi AP/web portal,
 * Android browser support and persistent high scores.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA      4
#define OLED_SCL      15
#define OLED_RESET    16
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

#define JOY_X         32
#define JOY_Y         33
#define JOY_BTN       25

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------- States --------------------
enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER };
enum GameType  {
  GAME_DINO=0,
  GAME_BOXES=1,
  GAME_FREE=2,
  GAME_PONG=3,
  GAME_SNAKE=4,
  GAME_BREAKOUT=5,
  GAME_INVADERS=6,
  GAME_ASTEROIDS=7,
  GAME_FLAPPY=8,
  GAME_RACING=9,
  GAME_MEMORY=10,
  GAME_COINS=11
};

const int GAME_COUNT = 12;
const int MENU_VISIBLE = 4;
const unsigned long FRAME_TIME_MS = 30;

GameState state = STATE_MENU;
GameType  currentGame = GAME_DINO;
int menuSelection = 0;
int gameOverSelection = 0;

// -------------------- Shared player (used by 1-3) --------------------
int manX = 30, manY = 0, manVy = 0;
bool facingRight = true;
int walkFrame = 0, frameCnt = 0;

const int GROUND_Y = 54;
const int JUMP_V   = 7;
const int GRAVITY  = 1;

// -------------------- Game 1 Dino --------------------
struct Spike { int x; bool active; };
Spike spikes[4];
int dinoScore = 0, groundScroll = 0;

// -------------------- Game 2 Boxes (with camera) --------------------
struct Box { int x, y; bool active; };
Box boxes[10];
int boxLevel = 0;
int cameraY = 0;
int highestReached = 0;

// -------------------- Game 4 Pong --------------------
int paddleX = 54;
int ballX = 64, ballY = 32;
int ballVX = 2, ballVY = -2;
int pongScore = 0;
int pongLives = 3;

// -------------------- Game 5 Snake --------------------
const int SNAKE_CELL = 4;
const int SNAKE_ORIGIN_Y = 10;
const int SNAKE_COLS = SCREEN_WIDTH / SNAKE_CELL;
const int SNAKE_ROWS = 13;
const int SNAKE_MAX = 120;

int snakeX[SNAKE_MAX];
int snakeY[SNAKE_MAX];
int snakeLength = 4;
int snakeDirX = 1;
int snakeDirY = 0;
int snakeNextDirX = 1;
int snakeNextDirY = 0;
int snakeFoodX = 20;
int snakeFoodY = 6;
int snakeScore = 0;
unsigned long snakeLastMove = 0;

// -------------------- Game 6 Breakout --------------------
const int BRICK_ROWS = 4;
const int BRICK_COLS = 8;
bool bricks[BRICK_ROWS][BRICK_COLS];
int breakoutPaddleX = 50;
int breakoutBallX = 64;
int breakoutBallY = 50;
int breakoutBallVX = 2;
int breakoutBallVY = -2;
int breakoutScore = 0;
int breakoutLives = 3;

// -------------------- Game 7 Space Invaders --------------------
const int INV_ROWS = 3, INV_COLS = 6, INV_MAX_BULLETS = 3;
bool invaders[INV_ROWS][INV_COLS];
int invaderPlayerX = 56, invaderDir = 1, invaderStep = 0;
int invaderOffsetX = 0, invaderDrop = 0;
int invaderBulletX[INV_MAX_BULLETS], invaderBulletY[INV_MAX_BULLETS];
bool invaderBulletActive[INV_MAX_BULLETS];
int invaderScore = 0, invaderLives = 3;

// -------------------- Game 8 Asteroids --------------------
struct Asteroid { int x,y,vx,vy,size; bool active; };
Asteroid asteroids[6];
int asteroidShipX=64, asteroidShipY=48, asteroidVX=0, asteroidVY=0;
int asteroidBulletX[3], asteroidBulletY[3], asteroidBulletVX[3], asteroidBulletVY[3];
bool asteroidBulletActive[3];
int asteroidScore=0, asteroidLives=3;

// -------------------- Game 9 Flappy --------------------
int flappyY=32, flappyV=0, flappyPipeX=128, flappyGapY=32;
int flappyScore=0, flappyLives=1;

// -------------------- Game 10 Racing --------------------
int raceCarX=60, raceRoadOffset=0, raceScore=0, raceSpeed=2;
struct RoadObstacle { int x,y; bool active; };
RoadObstacle raceObstacles[4];

// -------------------- Game 11 Memory --------------------
const int MEMORY_PAIRS=4;
int memoryCards[MEMORY_PAIRS*2];
bool memoryFound[MEMORY_PAIRS*2];
int memoryCursor=0, memoryFirst=-1, memorySecond=-1, memoryScore=0;
unsigned long memoryPauseUntil=0;

// -------------------- Game 12 Coin Collector --------------------
int coinPlayerX=64, coinPlayerY=32, coinScore=0;
int coinsX[8], coinsY[8];
bool coinsActive[8];

// Simple RAM high-score table. Persistent storage comes later.
int highScores[GAME_COUNT] = {0, 0, 0, 0, 0};

// -------------------- Menu --------------------
int menuTop = 0;
const char* gameNames[GAME_COUNT] = {
  "1. Dino Jump",
  "2. Box Climber",
  "3. Free Walk",
  "4. Ping Pong",
  "5. Snake",
  "6. Breakout",
  "7. Space Invaders",
  "8. Asteroids",
  "9. Flappy",
  "10. Racing",
  "11. Memory",
  "12. Coin Collector"
};

// -------------------- Timing --------------------
unsigned long lastFrame = 0;

// =====================================================
//  FORWARD DECLARATIONS
// =====================================================
void startGame();
void enterGameOver();
int currentScore();
void recordScore();
void updateSnake();
void drawSnake();
void updateBreakout();
void drawBreakout();
void updateInvaders(); void drawInvaders();
void updateAsteroids(); void drawAsteroids();
void updateFlappy(); void drawFlappy();
void updateRacing(); void drawRacing();
void updateMemory(); void drawMemory();
void updateCoins(); void drawCoins();

// =====================================================
//  JOYSTICK helpers (wide dead-zone)
// =====================================================
bool buttonPressed() {
  static bool last = false;
  bool now = digitalRead(JOY_BTN) == LOW;
  bool pressed = now && !last;
  last = now;
  return pressed;
}

int joyXDir() {
  int v = analogRead(JOY_X);
  if (v < 1000) return -1;
  if (v > 3000) return  1;
  return 0;
}

int joyYDir() {
  int v = analogRead(JOY_Y);
  if (v < 1000) return -1;
  if (v > 3000) return  1;
  return 0;
}

// =====================================================
//  DRAW MAN
// =====================================================
void drawMan(int x, int y, int frame, bool right) {
  int by = GROUND_Y - (y - cameraY);

  display.drawCircle(x, by - 14, 4, SSD1306_WHITE);
  display.drawPixel(x + (right ? 1 : -1), by - 15, SSD1306_WHITE);
  display.drawLine(x, by - 10, x, by - 3, SSD1306_WHITE);

  int arm = (frame % 2) ? 3 : -3;
  if (!right) arm = -arm;
  display.drawLine(x, by - 8, x - 5 + arm, by - 4, SSD1306_WHITE);
  display.drawLine(x, by - 8, x + 5 - arm, by - 4, SSD1306_WHITE);

  int lf, lb;
  switch (frame) {
    case 0: lf=-4; lb=4; break;
    case 1: lf=-2; lb=2; break;
    case 2: lf=4;  lb=-4; break;
    default:lf=2;  lb=-2; break;
  }
  if (!right) { lf=-lf; lb=-lb; }
  display.drawLine(x, by-3, x+lf, by, SSD1306_WHITE);
  display.drawLine(x, by-3, x+lb, by, SSD1306_WHITE);
}

// =====================================================
//  MENU
// =====================================================
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(12, 0);
  display.print("LA LINEA GAMES");

  display.setCursor(108, 0);
  display.print(menuSelection + 1);
  display.print("/");
  display.print(GAME_COUNT);

  for (int row = 0; row < MENU_VISIBLE; row++) {
    int index = menuTop + row;
    if (index >= GAME_COUNT) break;

    display.setCursor(5, 13 + row * 12);
    display.print(index == menuSelection ? "> " : "  ");
    display.print(gameNames[index]);
  }

  if (menuTop > 0) {
    display.setCursor(122, 13);
    display.print("^");
  }

  if (menuTop + MENU_VISIBLE < GAME_COUNT) {
    display.setCursor(122, 49);
    display.print("v");
  }

  display.display();
}

void updateMenu() {
  static int lastDir = 0;
  int dir = joyYDir();

  if (dir != 0 && lastDir == 0) {
    menuSelection = constrain(menuSelection + dir, 0, GAME_COUNT - 1);

    if (menuSelection < menuTop)
      menuTop = menuSelection;

    if (menuSelection >= menuTop + MENU_VISIBLE)
      menuTop = menuSelection - MENU_VISIBLE + 1;
  }

  lastDir = dir;

  if (buttonPressed()) {
    currentGame = (GameType)menuSelection;
    startGame();
  }
}

// =====================================================
//  GAME OVER
// =====================================================
int currentScore() {
  if (currentGame == GAME_DINO)  return dinoScore;
  if (currentGame == GAME_BOXES) return boxLevel;
  if (currentGame == GAME_PONG)  return pongScore;
  if (currentGame == GAME_SNAKE) return snakeScore;
  if (currentGame == GAME_BREAKOUT) return breakoutScore;
  if (currentGame == GAME_INVADERS) return invaderScore;
  if (currentGame == GAME_ASTEROIDS) return asteroidScore;
  if (currentGame == GAME_FLAPPY) return flappyScore;
  if (currentGame == GAME_RACING) return raceScore;
  if (currentGame == GAME_MEMORY) return memoryScore;
  if (currentGame == GAME_COINS) return coinScore;
  return 0;
}

void recordScore() {
  int score = currentScore();
  if (score > highScores[currentGame])
    highScores[currentGame] = score;
}

void enterGameOver() {
  recordScore();
  state = STATE_GAMEOVER;
  gameOverSelection = 0;
}

void drawGameOver() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(34, 2);
  display.print("GAME OVER");

  display.setCursor(20, 16);
  display.print("Score: ");
  display.print(currentScore());

  display.setCursor(20, 27);
  display.print("Best:  ");
  display.print(highScores[currentGame]);

  display.setCursor(10, 42);
  display.print(gameOverSelection == 0 ? "> Restart" : "  Restart");
  display.setCursor(10, 54);
  display.print(gameOverSelection == 1 ? "> Menu"    : "  Menu");
  display.display();
}

void updateGameOver() {
  static int lastDir = 0;
  int dir = joyYDir();
  if (dir != 0 && lastDir == 0)
    gameOverSelection = constrain(gameOverSelection + dir, 0, 1);
  lastDir = dir;

  if (buttonPressed()) {
    if (gameOverSelection == 0) startGame();
    else state = STATE_MENU;
  }
}

// =====================================================
//  START / RESET
// =====================================================
void startGame() {
  manX = 30; manY = 0; manVy = 0;
  facingRight = true; walkFrame = 0; frameCnt = 0;
  cameraY = 0;
  state = STATE_PLAYING;

  if (currentGame == GAME_DINO) {
    dinoScore = 0; groundScroll = 0;
    for (int i=0; i<4; i++) spikes[i].active = false;
  }
  else if (currentGame == GAME_BOXES) {
    boxLevel = 0; highestReached = 0; cameraY = 0;
    for (int i=0; i<10; i++) {
      boxes[i].active = true;
      boxes[i].x = 16 + (i%5)*22;
      boxes[i].y = 10 + (i/5)*16;
    }
  }
  else if (currentGame == GAME_PONG) {
    paddleX = 54;
    ballX = 64; ballY = 40;
    ballVX = 2; ballVY = -2;
    pongScore = 0; pongLives = 3;
  }
  else if (currentGame == GAME_SNAKE) {
    snakeLength = 4;
    snakeDirX = 1;
    snakeDirY = 0;
    snakeNextDirX = 1;
    snakeNextDirY = 0;
    snakeScore = 0;

    int startX = SNAKE_COLS / 2;
    int startY = SNAKE_ROWS / 2;

    for (int i = 0; i < snakeLength; i++) {
      snakeX[i] = startX - i;
      snakeY[i] = startY;
    }

    snakeFoodX = 20;
    snakeFoodY = 6;
    snakeLastMove = millis();
  }
  else if (currentGame == GAME_BREAKOUT) {
    breakoutPaddleX = 50; breakoutBallX = 64; breakoutBallY = 50;
    breakoutBallVX = random(0, 2) ? 2 : -2; breakoutBallVY = -2;
    breakoutScore = 0; breakoutLives = 3;
    for (int r=0;r<BRICK_ROWS;r++) for (int col=0;col<BRICK_COLS;col++) bricks[r][col]=true;
  }
  else if (currentGame == GAME_INVADERS) {
    invaderPlayerX=56; invaderDir=1; invaderStep=0; invaderOffsetX=0; invaderDrop=0; invaderScore=0; invaderLives=3;
    for(int r=0;r<INV_ROWS;r++) for(int col=0;col<INV_COLS;col++) invaders[r][col]=true;
    for(int i=0;i<INV_MAX_BULLETS;i++) invaderBulletActive[i]=false;
  }
  else if (currentGame == GAME_ASTEROIDS) {
    asteroidShipX=64; asteroidShipY=48; asteroidVX=0; asteroidVY=0; asteroidScore=0; asteroidLives=3;
    for(int i=0;i<6;i++){ asteroids[i].active=true; asteroids[i].x=random(0,128); asteroids[i].y=random(12,45); asteroids[i].vx=random(-2,3); asteroids[i].vy=random(-1,2); if(!asteroids[i].vx && !asteroids[i].vy) asteroids[i].vx=1; asteroids[i].size=4; }
    for(int i=0;i<3;i++) asteroidBulletActive[i]=false;
  }
  else if (currentGame == GAME_FLAPPY) {
    flappyY=32; flappyV=0; flappyPipeX=128; flappyGapY=random(22,44); flappyScore=0; flappyLives=1;
  }
  else if (currentGame == GAME_RACING) {
    raceCarX=60; raceRoadOffset=0; raceScore=0; raceSpeed=2;
    for(int i=0;i<4;i++){ raceObstacles[i].active=true; raceObstacles[i].x=random(43,86); raceObstacles[i].y=-i*35-random(0,20); }
  }
  else if (currentGame == GAME_MEMORY) {
    for(int i=0;i<MEMORY_PAIRS*2;i++){ memoryCards[i]=i/2; memoryFound[i]=false; }
    for(int i=0;i<20;i++){ int a=random(0,8), b=random(0,8); int t=memoryCards[a]; memoryCards[a]=memoryCards[b]; memoryCards[b]=t; }
    memoryCursor=0; memoryFirst=-1; memorySecond=-1; memoryScore=0; memoryPauseUntil=0;
  }
  else if (currentGame == GAME_COINS) {
    coinPlayerX=64; coinPlayerY=32; coinScore=0;
    for(int i=0;i<8;i++){ coinsActive[i]=true; coinsX[i]=random(8,120); coinsY[i]=random(14,54); }
  }
}

// =====================================================
//  GAME 1 – Dino
// =====================================================
void updateDino() {
  int dir = joyXDir();
  if (dir) { manX += dir*2; facingRight = dir>0; }
  manX = constrain(manX, 8, 60);

  if ((buttonPressed() || joyYDir()<0) && manY==0) manVy = JUMP_V;

  manY += manVy; manVy -= GRAVITY;
  if (manY < 0) { manY=0; manVy=0; }

  frameCnt++;
  if (manY==0 && dir && frameCnt%3==0) walkFrame = (walkFrame+1)%4;

  groundScroll = (groundScroll+2)%8;

  static unsigned long last=0;
  if (millis()-last > 1400) {
    for (int i=0;i<4;i++) if (!spikes[i].active) {
      spikes[i].x = SCREEN_WIDTH+5; spikes[i].active=true; break;
    }
    last = millis();
  }

  for (int i=0;i<4;i++) if (spikes[i].active) {
    spikes[i].x -= 3;
    if (spikes[i].x < manX+6 && spikes[i].x+8 > manX-4 && manY<9) {
      enterGameOver();
    }
    if (spikes[i].x < -12) { spikes[i].active=false; dinoScore++; }
  }
}

void drawDino() {
  display.clearDisplay();
  for (int x=-groundScroll; x<SCREEN_WIDTH; x+=8)
    display.drawLine(x, GROUND_Y, x+4, GROUND_Y, SSD1306_WHITE);

  for (int i=0;i<4;i++) if (spikes[i].active) {
    int sx=spikes[i].x;
    display.drawLine(sx,GROUND_Y, sx+4,GROUND_Y-9, SSD1306_WHITE);
    display.drawLine(sx+4,GROUND_Y-9, sx+8,GROUND_Y, SSD1306_WHITE);
  }
  drawMan(manX, manY, walkFrame, facingRight);
  display.setCursor(0,0); display.print("Score:"); display.print(dinoScore);
  display.display();
}

// =====================================================
//  GAME 2 – Box Climber
// =====================================================
void updateBoxes() {
  int dir = joyXDir();
  if (dir) { manX += dir*2; facingRight = dir>0; }
  manX = constrain(manX, 4, SCREEN_WIDTH-4);

  if ((buttonPressed() || joyYDir()<0) && manVy <= 0) {
    bool onSomething = (manY <= 2);
    for (int i=0;i<10;i++) {
      if (boxes[i].active && abs(manX-boxes[i].x)<11 && abs(manY-boxes[i].y)<5)
        onSomething = true;
    }
    if (onSomething) manVy = JUMP_V;
  }

  manY += manVy;
  manVy -= GRAVITY;

  for (int i=0;i<10;i++) {
    if (!boxes[i].active) continue;
    if (manVy <= 0 &&
        abs(manX - boxes[i].x) < 11 &&
        manY >= boxes[i].y && manY <= boxes[i].y + 6) {
      manY = boxes[i].y;
      manVy = 0;

      if (boxes[i].y > highestReached) {
        highestReached = boxes[i].y;
        boxLevel++;

        for (int j=0;j<10;j++)
          if (boxes[j].y < highestReached - 8) boxes[j].active = false;

        for (int k=0;k<3;k++) {
          for (int j=0;j<10;j++) if (!boxes[j].active) {
            boxes[j].active = true;
            boxes[j].x = 12 + random(0, 104);
            boxes[j].y = highestReached + 14 + k*13;
            break;
          }
        }
      }
    }
  }

  if (manY <= 0) {
    manY = 0;
    manVy = 0;
    if (highestReached > 15) {
      enterGameOver();
    }
  }

  int targetCam = manY - 28;
  if (targetCam > cameraY) cameraY = targetCam;
  if (cameraY < 0) cameraY = 0;

  if (manY < cameraY - 20) {
    enterGameOver();
  }

  frameCnt++;
  if (manVy==0 && dir && frameCnt%3==0) walkFrame=(walkFrame+1)%4;
}

void drawBoxes() {
  display.clearDisplay();

  int floorScreenY = GROUND_Y + cameraY;
  if (floorScreenY < SCREEN_HEIGHT)
    display.drawLine(0, floorScreenY, SCREEN_WIDTH-1, floorScreenY, SSD1306_WHITE);

  for (int i=0;i<10;i++) {
    if (!boxes[i].active) continue;
    int by = GROUND_Y - (boxes[i].y - cameraY);
    if (by > -10 && by < SCREEN_HEIGHT+10)
      display.fillRect(boxes[i].x-8, by-4, 16, 5, SSD1306_WHITE);
  }

  drawMan(manX, manY, walkFrame, facingRight);

  display.setCursor(0,0);
  display.print("Lv:");
  display.print(boxLevel);
  display.display();
}

// =====================================================
//  GAME 3 – Free Walk
// =====================================================
void updateFree() {
  int dir = joyXDir();
  if (dir) { manX += dir*2; facingRight = dir>0; }

  if ((buttonPressed() || joyYDir()<0) && manY==0) manVy = JUMP_V;

  manY += manVy; manVy -= GRAVITY;
  if (manY < 0) { manY=0; manVy=0; }

  if (manX > SCREEN_WIDTH+10) { state = STATE_MENU; return; }
  if (manX < 4) manX = 4;

  frameCnt++;
  if (manY==0 && dir && frameCnt%3==0) walkFrame=(walkFrame+1)%4;
}

void drawFree() {
  display.clearDisplay();
  display.drawLine(0, GROUND_Y, SCREEN_WIDTH-1, GROUND_Y, SSD1306_WHITE);
  drawMan(manX, manY, walkFrame, facingRight);
  display.setCursor(0,0);
  display.print("Walk off right = exit");
  display.display();
}

// =====================================================
//  GAME 4 – Ping Pong
// =====================================================
void updatePong() {
  int dir = joyXDir();
  paddleX += dir * 3;
  paddleX = constrain(paddleX, 0, SCREEN_WIDTH-20);

  ballX += ballVX;
  ballY += ballVY;

  if (ballX <= 0 || ballX >= SCREEN_WIDTH-2) ballVX = -ballVX;
  if (ballY <= 0) {
    ballVY = -ballVY;
    pongScore++;
  }

  if (ballY >= 56 && ballY <= 60 &&
      ballX >= paddleX && ballX <= paddleX+20) {
    ballVY = -abs(ballVY);
    ballVX += (ballX - (paddleX+10)) / 4;
    ballVX = constrain(ballVX, -3, 3);
  }

  if (ballY > SCREEN_HEIGHT) {
    pongLives--;
    ballX = 64; ballY = 40;
    ballVX = (random(0,2)?2:-2);
    ballVY = -2;
    if (pongLives <= 0) {
      enterGameOver();
    }
  }
}

void drawPong() {
  display.clearDisplay();

  display.fillRect(paddleX, 58, 20, 3, SSD1306_WHITE);
  display.fillRect(ballX, ballY, 3, 3, SSD1306_WHITE);

  display.setCursor(0,0);
  display.print("S:");
  display.print(pongScore);
  display.print("  L:");
  display.print(pongLives);

  display.display();
}

// =====================================================
//  GAME 5 – Snake
// =====================================================
void placeSnakeFood() {
  for (int attempts = 0; attempts < 100; attempts++) {
    int fx = random(0, SNAKE_COLS);
    int fy = random(0, SNAKE_ROWS);

    bool occupied = false;
    for (int i = 0; i < snakeLength; i++) {
      if (snakeX[i] == fx && snakeY[i] == fy) {
        occupied = true;
        break;
      }
    }

    if (!occupied) {
      snakeFoodX = fx;
      snakeFoodY = fy;
      return;
    }
  }
}

void updateSnake() {
  // Read joystick direction continuously, but only accept 90-degree turns.
  int x = joyXDir();
  int y = joyYDir();

  if (x != 0 && snakeDirX == 0) {
    snakeNextDirX = x;
    snakeNextDirY = 0;
  }
  else if (y != 0 && snakeDirY == 0) {
    snakeNextDirX = 0;
    snakeNextDirY = y;
  }

  unsigned long now = millis();

  int moveInterval = 150 - (snakeScore * 4);
  if (moveInterval < 70) moveInterval = 70;

  if (now - snakeLastMove < (unsigned long)moveInterval)
    return;

  snakeLastMove = now;
  snakeDirX = snakeNextDirX;
  snakeDirY = snakeNextDirY;

  int newX = snakeX[0] + snakeDirX;
  int newY = snakeY[0] + snakeDirY;

  // Wrap-around keeps the first Snake version simple and arcade-like.
  if (newX < 0) newX = SNAKE_COLS - 1;
  if (newX >= SNAKE_COLS) newX = 0;
  if (newY < 0) newY = SNAKE_ROWS - 1;
  if (newY >= SNAKE_ROWS) newY = 0;

  bool ateFood = (newX == snakeFoodX && newY == snakeFoodY);

  // If not eating, the old tail moves away, so it is not a collision target.
  int collisionLength = ateFood ? snakeLength : snakeLength - 1;

  for (int i = 0; i < collisionLength; i++) {
    if (snakeX[i] == newX && snakeY[i] == newY) {
      enterGameOver();
      return;
    }
  }

  int newLength = snakeLength + (ateFood ? 1 : 0);

  if (newLength > SNAKE_MAX)
    newLength = SNAKE_MAX;

  for (int i = newLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  snakeX[0] = newX;
  snakeY[0] = newY;
  snakeLength = newLength;

  if (ateFood) {
    snakeScore++;
    placeSnakeFood();
  }
}

void drawSnake() {
  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("Snake:");
  display.print(snakeScore);

  display.setCursor(78, 0);
  display.print("Best:");
  display.print(highScores[GAME_SNAKE]);

  // Playfield border.
  display.drawRect(0, SNAKE_ORIGIN_Y - 1,
                   SCREEN_WIDTH, SNAKE_ROWS * SNAKE_CELL + 2,
                   SSD1306_WHITE);

  // Food.
  int foodPx = snakeFoodX * SNAKE_CELL;
  int foodPy = SNAKE_ORIGIN_Y + snakeFoodY * SNAKE_CELL;
  display.fillRect(foodPx, foodPy, SNAKE_CELL, SNAKE_CELL, SSD1306_WHITE);

  // Snake.
  for (int i = snakeLength - 1; i >= 0; i--) {
    int px = snakeX[i] * SNAKE_CELL;
    int py = SNAKE_ORIGIN_Y + snakeY[i] * SNAKE_CELL;

    if (i == 0)
      display.fillRect(px, py, SNAKE_CELL, SNAKE_CELL, SSD1306_WHITE);
    else
      display.drawRect(px, py, SNAKE_CELL, SNAKE_CELL, SSD1306_WHITE);
  }

  display.display();
}

// =====================================================
//  GAME 6 – Breakout
// =====================================================
void updateBreakout() {
  int dir = joyXDir();
  breakoutPaddleX += dir * 3;
  breakoutPaddleX = constrain(breakoutPaddleX, 0, SCREEN_WIDTH - 24);

  breakoutBallX += breakoutBallVX;
  breakoutBallY += breakoutBallVY;

  if (breakoutBallX <= 0) {
    breakoutBallX = 0;
    breakoutBallVX = abs(breakoutBallVX);
  }
  if (breakoutBallX >= SCREEN_WIDTH - 3) {
    breakoutBallX = SCREEN_WIDTH - 3;
    breakoutBallVX = -abs(breakoutBallVX);
  }

  if (breakoutBallY <= 12) {
    breakoutBallY = 12;
    breakoutBallVY = abs(breakoutBallVY);
  }

  // Paddle collision.
  if (breakoutBallVY > 0 &&
      breakoutBallY + 3 >= 57 &&
      breakoutBallY <= 61 &&
      breakoutBallX + 3 >= breakoutPaddleX &&
      breakoutBallX <= breakoutPaddleX + 24) {
    breakoutBallY = 54;
    breakoutBallVY = -abs(breakoutBallVY);

    int offset = breakoutBallX - (breakoutPaddleX + 12);
    breakoutBallVX = constrain(2 + (offset / 6), -3, 3);
    if (breakoutBallVX == 0) breakoutBallVX = (offset < 0) ? -1 : 1;
  }

  // Brick collision.
  int brickW = 16;
  int brickH = 6;
  int brickTop = 14;

  int col = breakoutBallX / brickW;
  int row = (breakoutBallY - brickTop) / brickH;

  if (col >= 0 && col < BRICK_COLS &&
      row >= 0 && row < BRICK_ROWS &&
      bricks[row][col]) {
    bricks[row][col] = false;
    breakoutScore++;
    breakoutBallVY = -breakoutBallVY;

    bool anyLeft = false;
    for (int r = 0; r < BRICK_ROWS; r++)
      for (int c = 0; c < BRICK_COLS; c++)
        if (bricks[r][c]) anyLeft = true;

    if (!anyLeft) {
      enterGameOver();
      return;
    }
  }

  if (breakoutBallY > SCREEN_HEIGHT) {
    breakoutLives--;

    if (breakoutLives <= 0) {
      enterGameOver();
      return;
    }

    breakoutBallX = 64;
    breakoutBallY = 50;
    breakoutBallVX = random(0, 2) ? 2 : -2;
    breakoutBallVY = -2;
  }
}

void drawBreakout() {
  display.clearDisplay();

  // Bricks.
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      if (bricks[r][c]) {
        display.fillRect(c * 16, 14 + r * 6, 15, 5, SSD1306_WHITE);
      }
    }
  }

  // Paddle and ball.
  display.fillRect(breakoutPaddleX, 58, 24, 3, SSD1306_WHITE);
  display.fillRect(breakoutBallX, breakoutBallY, 3, 3, SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("S:");
  display.print(breakoutScore);
  display.print(" L:");
  display.print(breakoutLives);

  display.display();
}

// =====================================================
//  GAME 7 – Space Invaders
// =====================================================
void updateInvaders(){
  int dir=joyXDir(); invaderPlayerX+=dir*3; invaderPlayerX=constrain(invaderPlayerX,4,116);
  if(buttonPressed()){
    for(int i=0;i<INV_MAX_BULLETS;i++) if(!invaderBulletActive[i]){invaderBulletActive[i]=true;invaderBulletX[i]=invaderPlayerX;invaderBulletY[i]=54;break;}
  }
  invaderStep++;
  if(invaderStep>=8){
    invaderStep=0;
    bool edge=false;
    for(int r=0;r<INV_ROWS;r++) for(int col=0;col<INV_COLS;col++) if(invaders[r][col]){
      int ix=10+col*20+invaderOffsetX;
      if((invaderDir>0 && ix>116)||(invaderDir<0 && ix<12)) edge=true;
    }
    if(edge){ invaderDir=-invaderDir; invaderDrop+=3; }
    else invaderOffsetX+=invaderDir*3;
  }
  for(int i=0;i<INV_MAX_BULLETS;i++) if(invaderBulletActive[i]){
    invaderBulletY[i]-=5;
    if(invaderBulletY[i]<8){invaderBulletActive[i]=false;continue;}

    bool hit = false;
    for(int r=0;r<INV_ROWS && !hit;r++) {
      for(int col=0;col<INV_COLS;col++) {
        if(!invaders[r][col]) continue;
        int ix=10+col*20+invaderOffsetX;
        int iy=12+r*9+invaderDrop;
        if(abs(invaderBulletX[i]-ix)<7 && abs(invaderBulletY[i]-iy)<5){
          invaders[r][col]=false;
          invaderBulletActive[i]=false;
          invaderScore++;
          hit = true;
          break;
        }
      }
    }
  }
  for(int r=0;r<INV_ROWS;r++)for(int col=0;col<INV_COLS;col++)if(invaders[r][col]){
    int iy=12+r*9+invaderDrop;
    if(iy>48){enterGameOver();return;}
  }
  bool left=false;for(int r=0;r<INV_ROWS;r++)for(int col=0;col<INV_COLS;col++)if(invaders[r][col])left=true;
  if(!left)enterGameOver();
}
void drawInvaders(){
  display.clearDisplay();
  for(int r=0;r<INV_ROWS;r++)for(int col=0;col<INV_COLS;col++)if(invaders[r][col]){
    int x=10+col*20+invaderOffsetX,y=12+r*9+invaderDrop;
    display.fillRect(x-6,y-3,12,5,SSD1306_WHITE); display.drawPixel(x-4,y+3,SSD1306_WHITE); display.drawPixel(x+4,y+3,SSD1306_WHITE);
  }
  display.fillRect(invaderPlayerX-6,57,12,3,SSD1306_WHITE);
  for(int i=0;i<INV_MAX_BULLETS;i++)if(invaderBulletActive[i])display.fillRect(invaderBulletX[i],invaderBulletY[i],2,4,SSD1306_WHITE);
  display.setCursor(0,0);display.print("S:");display.print(invaderScore);display.print(" L:");display.print(invaderLives);display.display();
}

// =====================================================
//  GAME 8 – Asteroids
// =====================================================
void updateAsteroids(){
  int dx=joyXDir(), dy=joyYDir(); asteroidShipX+=dx*2; asteroidShipY+=dy*2; asteroidShipX=constrain(asteroidShipX,8,120); asteroidShipY=constrain(asteroidShipY,12,56);
  if(buttonPressed())for(int i=0;i<3;i++)if(!asteroidBulletActive[i]){asteroidBulletActive[i]=true;asteroidBulletX[i]=asteroidShipX;asteroidBulletY[i]=asteroidShipY-5;asteroidBulletVX[i]=0;asteroidBulletVY[i]=-4;break;}
  for(int i=0;i<6;i++)if(asteroids[i].active){
    asteroids[i].x+=asteroids[i].vx;asteroids[i].y+=asteroids[i].vy;
    if(asteroids[i].x<0)asteroids[i].x=127;if(asteroids[i].x>127)asteroids[i].x=0;
    if(asteroids[i].y<8)asteroids[i].y=60;if(asteroids[i].y>60)asteroids[i].y=8;
    if(abs(asteroids[i].x-asteroidShipX)<6&&abs(asteroids[i].y-asteroidShipY)<6){asteroids[i].x=random(0,128);asteroids[i].y=random(12,45);asteroidLives--;if(asteroidLives<=0){enterGameOver();return;}}
  }
  for(int b=0;b<3;b++)if(asteroidBulletActive[b]){
    asteroidBulletX[b]+=asteroidBulletVX[b];asteroidBulletY[b]+=asteroidBulletVY[b];
    if(asteroidBulletY[b]<8){asteroidBulletActive[b]=false;continue;}
    for(int i=0;i<6;i++)if(asteroids[i].active&&abs(asteroidBulletX[b]-asteroids[i].x)<6&&abs(asteroidBulletY[b]-asteroids[i].y)<6){
      asteroids[i].active=false;asteroidBulletActive[b]=false;asteroidScore++;break;
    }
  }
  bool left=false;for(int i=0;i<6;i++)if(asteroids[i].active)left=true;
  if(!left){for(int i=0;i<6;i++){asteroids[i].active=true;asteroids[i].x=random(0,128);asteroids[i].y=random(12,45);asteroids[i].vx=random(-2,3);asteroids[i].vy=random(-1,2);} }
}
void drawAsteroids(){
  display.clearDisplay();
  display.drawCircle(asteroidShipX,asteroidShipY,4,SSD1306_WHITE);display.drawLine(asteroidShipX,asteroidShipY-4,asteroidShipX,asteroidShipY-7,SSD1306_WHITE);
  for(int i=0;i<6;i++)if(asteroids[i].active)display.drawCircle(asteroids[i].x,asteroids[i].y,asteroids[i].size,SSD1306_WHITE);
  for(int i=0;i<3;i++)if(asteroidBulletActive[i])display.drawPixel(asteroidBulletX[i],asteroidBulletY[i],SSD1306_WHITE);
  display.setCursor(0,0);display.print("S:");display.print(asteroidScore);display.print(" L:");display.print(asteroidLives);display.display();
}

// =====================================================
//  GAME 9 – Flappy
// =====================================================
void updateFlappy(){
  if(buttonPressed()||joyYDir()<0)flappyV=-5;
  flappyV+=1;flappyY+=flappyV;
  flappyPipeX-=2;
  if(flappyPipeX<-12){flappyPipeX=128;flappyGapY=random(20,45);flappyScore++;}
  int gapTop=flappyGapY-10,gapBot=flappyGapY+10;
  if(flappyY<8||flappyY>58||(flappyPipeX<24&&flappyPipeX>8&&(flappyY<gapTop||flappyY>gapBot))){enterGameOver();return;}
}
void drawFlappy(){
  display.clearDisplay();display.fillCircle(18,flappyY,3,SSD1306_WHITE);
  int gapTop=flappyGapY-10,gapBot=flappyGapY+10;display.fillRect(flappyPipeX,8,10,gapTop-8,SSD1306_WHITE);display.fillRect(flappyPipeX,gapBot,10,60-gapBot,SSD1306_WHITE);
  display.setCursor(0,0);display.print("S:");display.print(flappyScore);display.display();
}

// =====================================================
//  GAME 10 – Racing
// =====================================================
void updateRacing(){
  int dir=joyXDir();raceCarX+=dir*3;raceCarX=constrain(raceCarX,43,85);raceRoadOffset=(raceRoadOffset+raceSpeed)%10;
  for(int i=0;i<4;i++){raceObstacles[i].y+=raceSpeed;if(raceObstacles[i].y>64){raceObstacles[i].y=-random(15,45);raceObstacles[i].x=random(45,82);raceScore++;if(raceScore%8==0&&raceSpeed<5)raceSpeed++;}if(raceObstacles[i].y>50&&raceObstacles[i].y<62&&abs(raceObstacles[i].x-raceCarX)<8){enterGameOver();return;}}
}
void drawRacing(){
  display.clearDisplay();display.drawLine(40,0,40,63,SSD1306_WHITE);display.drawLine(88,0,88,63,SSD1306_WHITE);
  for(int y=-10;y<64;y+=10)display.drawLine(63,y+raceRoadOffset,63,y+5+raceRoadOffset,SSD1306_WHITE);
  display.fillRect(raceCarX-5,54,10,8,SSD1306_WHITE);for(int i=0;i<4;i++)display.fillRect(raceObstacles[i].x-4,raceObstacles[i].y,8,7,SSD1306_WHITE);
  display.setCursor(0,0);display.print("S:");display.print(raceScore);display.display();
}

// =====================================================
//  GAME 11 – Memory
// =====================================================
void updateMemory(){
  if(millis()<memoryPauseUntil)return;
  int x=joyXDir(), y=joyYDir();
  if(x||y){int old=memoryCursor;if(x>0)memoryCursor++;if(x<0)memoryCursor--;if(y>0)memoryCursor+=4;if(y<0)memoryCursor-=4;memoryCursor=constrain(memoryCursor,0,7);if(memoryCursor!=old)delay(90);}
  if(buttonPressed()&&!memoryFound[memoryCursor]){
    if(memoryFirst<0)memoryFirst=memoryCursor;
    else if(memorySecond<0&&memoryCursor!=memoryFirst){memorySecond=memoryCursor;memoryPauseUntil=millis()+600;}
  }
  if(memorySecond>=0&&millis()>=memoryPauseUntil){
    if(memoryCards[memoryFirst]==memoryCards[memorySecond]){memoryFound[memoryFirst]=true;memoryFound[memorySecond]=true;memoryScore++;}
    memoryFirst=-1;memorySecond=-1;
    bool done=true;for(int i=0;i<8;i++)if(!memoryFound[i])done=false;if(done)enterGameOver();
  }
}
void drawMemory(){
  display.clearDisplay();
  for(int i=0;i<8;i++){int x=8+(i%4)*30,y=12+(i/4)*22;if(memoryFound[i]){display.fillRect(x,y,20,16,SSD1306_WHITE);display.setTextColor(SSD1306_BLACK);display.setCursor(x+7,y+4);display.print(memoryCards[i]+1);display.setTextColor(SSD1306_WHITE);}else if(i==memoryFirst||i==memorySecond){display.drawRect(x,y,20,16,SSD1306_WHITE);display.setCursor(x+7,y+4);display.print(memoryCards[i]+1);}else display.drawRect(x,y,20,16,SSD1306_WHITE);if(i==memoryCursor)display.drawRect(x-2,y-2,24,20,SSD1306_WHITE);}
  display.setCursor(0,0);display.print("Pairs:");display.print(memoryScore);display.display();
}

// =====================================================
//  GAME 12 – Coin Collector
// =====================================================
void updateCoins(){
  int dx=joyXDir(),dy=joyYDir();coinPlayerX+=dx*3;coinPlayerY+=dy*3;coinPlayerX=constrain(coinPlayerX,5,123);coinPlayerY=constrain(coinPlayerY,12,59);
  for(int i=0;i<8;i++)if(coinsActive[i]&&abs(coinPlayerX-coinsX[i])<6&&abs(coinPlayerY-coinsY[i])<6){coinsActive[i]=false;coinScore++;}
  bool left=false;for(int i=0;i<8;i++)if(coinsActive[i])left=true;
  if(!left)enterGameOver();
}
void drawCoins(){
  display.clearDisplay();display.drawRect(2,10,124,53,SSD1306_WHITE);display.fillRect(coinPlayerX-3,coinPlayerY-3,6,6,SSD1306_WHITE);
  for(int i=0;i<8;i++)if(coinsActive[i])display.drawCircle(coinsX[i],coinsY[i],3,SSD1306_WHITE);
  display.setCursor(0,0);display.print("Coins:");display.print(coinScore);display.display();
}

// =====================================================
//  MAIN
// =====================================================
void setup() {
  Serial.begin(115200);
  pinMode(JOY_BTN, INPUT_PULLUP);
  Wire.begin(OLED_SDA, OLED_SCL);
  randomSeed(micros());

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) while(1) delay(100);
  display.clearDisplay();
  display.display();
  state = STATE_MENU;
}

void loop() {
  unsigned long now = millis();

  if (now - lastFrame < FRAME_TIME_MS)
    return;

  lastFrame = now;

  switch (state) {
    case STATE_MENU:
      updateMenu();
      drawMenu();
      break;

    case STATE_PLAYING:
      if      (currentGame == GAME_DINO)  { updateDino();  drawDino();  }
      else if (currentGame == GAME_BOXES) { updateBoxes(); drawBoxes(); }
      else if (currentGame == GAME_FREE)  { updateFree();  drawFree();  }
      else if (currentGame == GAME_PONG)  { updatePong();  drawPong();  }
      else if (currentGame == GAME_SNAKE) { updateSnake(); drawSnake(); }
      else if (currentGame == GAME_BREAKOUT) { updateBreakout(); drawBreakout(); }
      else if (currentGame == GAME_INVADERS) { updateInvaders(); drawInvaders(); }
      else if (currentGame == GAME_ASTEROIDS) { updateAsteroids(); drawAsteroids(); }
      else if (currentGame == GAME_FLAPPY) { updateFlappy(); drawFlappy(); }
      else if (currentGame == GAME_RACING) { updateRacing(); drawRacing(); }
      else if (currentGame == GAME_MEMORY) { updateMemory(); drawMemory(); }
      else if (currentGame == GAME_COINS) { updateCoins(); drawCoins(); }
      break;

    case STATE_GAMEOVER:
      updateGameOver();
      drawGameOver();
      break;
  }
}
