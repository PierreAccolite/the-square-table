/*
 * POCKET ARCADE
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
 *   13. Tetris
 *   14. Lunar Lander
 *   15. Frogger
 *   16. Tron
 *
 * V4 adds:
 *   - Scrollable game menu
 *   - Explicit function prototypes
 *   - RAM high-score framework
 *   - Non-blocking frame timing foundation
 *   - First additional games: Snake + Breakout
 *
 * Standalone ESP32 arcade console.
 * Wi-Fi, Bluetooth, and web-game control are intentionally not used.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RESET 16
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define JOY_X 32
#define JOY_Y 33
#define JOY_BTN 25
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
  GAME_COINS=11,
  GAME_TETRIS=12,
  GAME_LUNAR=13,
  GAME_FROGGER=14,
  GAME_TRON=15
};

const int GAME_COUNT = 16;
const int MENU_COUNT = GAME_COUNT;
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
int dinoSpeed = 3;
int dinoCloudX = 92;
unsigned long dinoLastSpawn = 0;

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


// -------------------- Game 13 Tetris --------------------
const int TETRIS_W=10,TETRIS_H=16; byte tetrisBoard[TETRIS_H][TETRIS_W];
int tetrisPiece=0,tetrisRot=0,tetrisX=3,tetrisY=0,tetrisScore=0,tetrisLines=0; unsigned long tetrisLastDrop=0;
const uint16_t tetrisShapes[7][4]={{0x0F00,0x2222,0x0F00,0x2222},{0x6600,0x6600,0x6600,0x6600},{0x6C00,0x4620,0x6C00,0x4620},{0xC600,0x2640,0xC600,0x2640},{0x8E00,0x4C40,0xE200,0x44C0},{0x2E00,0x4460,0xE800,0xC440},{0x4E00,0x4640,0xE400,0x4C40}};
// -------------------- Game 14 Lunar Lander --------------------
float lunarX=64,lunarY=20,lunarVX=0,lunarVY=0; int lunarFuel=100,lunarScore=0,lunarPadX=52,lunarPadW=24;
// -------------------- Game 15 Frogger --------------------
int frogX=64,frogY=56,frogScore=0,frogLives=3; struct FrogCar{int x,y,v;}; FrogCar frogCars[5];
// -------------------- Game 16 Tron --------------------
const int TRON_W=32,TRON_H=14; bool tronTrail[TRON_H][TRON_W]; int tronX=5,tronY=7,tronDX=1,tronDY=0,tronScore=0,tronEnemyX=26,tronEnemyY=6,tronEnemyDX=-1,tronEnemyDY=0;

// Simple RAM high-score table. Persistent storage comes later.
int highScores[GAME_COUNT] = {0};

// -------------------- Menu --------------------
int menuTop = 0;
const char* gameNames[MENU_COUNT] = {
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
  "12. Coin Collector",
  "13. Tetris",
  "14. Lunar Lander",
  "15. Frogger",
  "16. Tron"
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
void updateTetris(); void drawTetris(); void updateLunar(); void drawLunar();
void updateFrogger(); void drawFrogger(); void updateTron(); void drawTron();

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
  if (v > 3000) return 1;
  return 0;
}

int joyYDir() {
  int v = analogRead(JOY_Y);
  if (v < 1000) return -1;
  if (v > 3000) return 1;
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
  display.print("POCKET ARCADE");

  display.setCursor(96, 0);
  display.print(menuSelection + 1);
  display.print("/");
  display.print(MENU_COUNT);

  for (int row = 0; row < MENU_VISIBLE; row++) {
    int index = menuTop + row;
    if (index >= MENU_COUNT) break;

    display.setCursor(5, 13 + row * 12);
    display.print(index == menuSelection ? "> " : "  ");
    display.print(gameNames[index]);
  }

  if (menuTop > 0) {
    display.setCursor(122, 13);
    display.print("^");
  }

  if (menuTop + MENU_VISIBLE < MENU_COUNT) {
    display.setCursor(122, 49);
    display.print("v");
  }

  display.display();
}

void updateMenu() {
  static int lastDir = 0;
  int dir = joyYDir();

  if (dir != 0 && lastDir == 0) {
    menuSelection = constrain(menuSelection + dir, 0, MENU_COUNT - 1);

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
  if (currentGame == GAME_TETRIS) return tetrisScore;
  if (currentGame == GAME_LUNAR) return lunarScore;
  if (currentGame == GAME_FROGGER) return frogScore;
  if (currentGame == GAME_TRON) return tronScore;
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
    dinoScore = 0;
    groundScroll = 0;
    dinoSpeed = 3;
    dinoCloudX = 92;
    dinoLastSpawn = millis();
    manX = 28;
    manY = 0;
    manVy = 0;
    walkFrame = 0;
    frameCnt = 0;
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
  else if(currentGame==GAME_TETRIS){memset(tetrisBoard,0,sizeof(tetrisBoard));tetrisPiece=random(0,7);tetrisRot=0;tetrisX=3;tetrisY=0;tetrisScore=0;tetrisLines=0;tetrisLastDrop=millis();}
  else if(currentGame==GAME_LUNAR){lunarX=64;lunarY=18;lunarVX=lunarVY=0;lunarFuel=100;lunarScore=0;lunarPadX=random(8,96);}
  else if(currentGame==GAME_FROGGER){frogX=64;frogY=56;frogScore=0;frogLives=3;for(int i=0;i<5;i++){frogCars[i].x=random(0,128);frogCars[i].y=16+i*9;frogCars[i].v=(i&1)?-1:1;}}
  else if(currentGame==GAME_TRON){memset(tronTrail,0,sizeof(tronTrail));tronX=5;tronY=7;tronDX=1;tronDY=0;tronEnemyX=26;tronEnemyY=6;tronEnemyDX=-1;tronEnemyDY=0;tronScore=0;}
}

// =====================================================
//  GAME 1 – Dino Jump (WALL·E inspired)
//
//  Controls:
//    Joystick X = move left/right
//    Joystick Y UP = jump
//    Joystick button = jump
// =====================================================
void updateDino() {
  int dir = joyXDir();

  if (dir) {
    manX += dir * 2;
    facingRight = dir > 0;
  }
  manX = constrain(manX, 10, 58);

  if ((buttonPressed() || joyYDir() < 0) && manY == 0)
    manVy = JUMP_V;

  manY += manVy;
  manVy -= GRAVITY;
  if (manY < 0) {
    manY = 0;
    manVy = 0;
  }

  frameCnt++;
  if (manY == 0 && dir && frameCnt % 3 == 0)
    walkFrame = (walkFrame + 1) % 4;

  groundScroll = (groundScroll + dinoSpeed) % 8;

  dinoCloudX--;
  if (dinoCloudX < -35)
    dinoCloudX = SCREEN_WIDTH + random(10, 50);

  unsigned long spawnInterval = max(650UL, 1250UL - (unsigned long)dinoScore * 18UL);

  if (millis() - dinoLastSpawn > spawnInterval) {
    for (int i = 0; i < 4; i++) {
      if (!spikes[i].active) {
        spikes[i].x = SCREEN_WIDTH + 4;
        spikes[i].active = true;
        break;
      }
    }
    dinoLastSpawn = millis();
  }

  for (int i = 0; i < 4; i++) {
    if (!spikes[i].active) continue;

    spikes[i].x -= dinoSpeed;

    if (spikes[i].x < manX + 7 &&
        spikes[i].x + 8 > manX - 5 &&
        manY < 10) {
      enterGameOver();
      return;
    }

    if (spikes[i].x < -12) {
      spikes[i].active = false;
      dinoScore++;

      if (dinoScore % 5 == 0 && dinoSpeed < 6)
        dinoSpeed++;
    }
  }
}

void drawDinoCharacter(int x, int y, int frame) {
  int base = GROUND_Y - y;
  int legA = (frame % 2) ? 2 : -2;
  int legB = -legA;

  // Body and head.
  display.fillRect(x - 5, base - 15, 11, 12, SSD1306_WHITE);
  display.fillRect(x + 2, base - 22, 9, 9, SSD1306_WHITE);
  display.fillRect(x + 9, base - 19, 5, 4, SSD1306_WHITE);

  // Eye and mouth.
  display.drawPixel(x + 8, base - 19, SSD1306_BLACK);
  display.drawPixel(x + 13, base - 16, SSD1306_BLACK);

  // Tail and arm.
  display.drawLine(x - 5, base - 12, x - 10, base - 9, SSD1306_WHITE);
  display.drawLine(x - 10, base - 9, x - 13, base - 9, SSD1306_WHITE);
  display.drawLine(x + 3, base - 11, x + 7, base - 8, SSD1306_WHITE);

  // Legs.
  display.drawLine(x - 2, base - 3, x - 2 + legA, base, SSD1306_WHITE);
  display.drawLine(x + 3, base - 3, x + 3 + legB, base, SSD1306_WHITE);
}

void drawDinoCactus(int x, int variant) {
  int h = (variant == 0) ? 10 : 13;
  int trunk = (variant == 0) ? 4 : 5;

  display.fillRect(x, GROUND_Y - h, trunk, h, SSD1306_WHITE);
  display.fillRect(x - 3, GROUND_Y - h + 4, 3, 4, SSD1306_WHITE);
  display.fillRect(x + trunk, GROUND_Y - h + 2, 3, 5, SSD1306_WHITE);
}

void drawDinoCloud(int x, int y) {
  display.drawCircle(x, y, 4, SSD1306_WHITE);
  display.drawCircle(x + 5, y - 2, 5, SSD1306_WHITE);
  display.drawCircle(x + 11, y, 4, SSD1306_WHITE);
  display.drawLine(x - 2, y + 3, x + 14, y + 3, SSD1306_WHITE);
}

void drawDino() {
  display.clearDisplay();

  drawDinoCloud(dinoCloudX, 17);

  for (int x = -groundScroll; x < SCREEN_WIDTH; x += 8)
    display.drawLine(x, GROUND_Y, x + 4, GROUND_Y, SSD1306_WHITE);

  for (int i = 0; i < 4; i++) {
    if (!spikes[i].active) continue;
    drawDinoCactus(spikes[i].x, i % 2);
  }

  drawDinoCharacter(manX, manY, walkFrame);

  display.setCursor(0, 0);
  display.print("S:");
  display.print(dinoScore);
  display.setCursor(90, 0);
  display.print("x");
  display.print(dinoSpeed - 2);

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

  if (breakoutBallY > SCREEN_HEIGHT) {
    if (--breakoutLives <= 0) { enterGameOver(); return; }
    breakoutBallX=64; breakoutBallY=50; breakoutBallVX=random(0,2)?2:-2; breakoutBallVY=-2;
    return;
  }

  // Brick collision.
  int brickW = 16;
  int brickH = 6;
  int brickTop = 14;


  int col = breakoutBallX / brickW;
  int row = (breakoutBallY - brickTop) / brickH;

  if (col >= 0 && col < BRICK_COLS && row >= 0 && row < BRICK_ROWS && bricks[row][col]) {
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
}

void drawBreakout() {
  display.clearDisplay();
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      if (bricks[r][c])
        display.fillRect(c * 16 + 1, 14 + r * 6, 14, 5, SSD1306_WHITE);
  display.fillRect(breakoutPaddleX, 58, 24, 3, SSD1306_WHITE);
  display.fillRect(breakoutBallX, breakoutBallY, 3, 3, SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("S:");
  display.print(breakoutScore);
  display.print(" L:");
  display.print(breakoutLives);
  display.display();
}

void fireInvader() {
  for (int i = 0; i < INV_MAX_BULLETS; i++) {
    if (!invaderBulletActive[i]) {
      invaderBulletActive[i] = true;
      invaderBulletX[i] = invaderPlayerX + 4;
      invaderBulletY[i] = 53;
      return;
    }
  }
}

void updateInvaders() {
  int d = joyXDir();
  invaderPlayerX = constrain(invaderPlayerX + d * 2, 2, 118);
  if (buttonPressed()) fireInvader();

  static uint32_t t = 0;
  if (millis() - t > 350) {
    t = millis();
    bool edge = false;
    for (int r = 0; r < INV_ROWS; r++)
      for (int c = 0; c < INV_COLS; c++)
        if (invaders[r][c]) {
          int x = 12 + c * 16 + invaderOffsetX;
          if ((invaderDir > 0 && x > 112) || (invaderDir < 0 && x < 2)) edge = true;
        }
    if (edge) {
      invaderDir = -invaderDir;
      invaderDrop += 4;
    } else {
      invaderOffsetX += invaderDir * 3;
    }
  }

  for (int i = 0; i < INV_MAX_BULLETS; i++) {
    if (!invaderBulletActive[i]) continue;
    invaderBulletY[i] -= 4;
    if (invaderBulletY[i] < 8) {
      invaderBulletActive[i] = false;
      continue;
    }
    for (int r = 0; r < INV_ROWS; r++)
      for (int c = 0; c < INV_COLS; c++)
        if (invaders[r][c]) {
          int x = 12 + c * 16 + invaderOffsetX;
          int y = 15 + r * 9 + invaderDrop;
          if (invaderBulletX[i] >= x && invaderBulletX[i] <= x + 11 &&
              invaderBulletY[i] >= y && invaderBulletY[i] <= y + 6) {
            invaders[r][c] = false;
            invaderBulletActive[i] = false;
            invaderScore++;
          }
        }
  }

  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (invaders[r][c] && 15 + r * 9 + invaderDrop > 50) {
        enterGameOver();
        return;
      }

  bool left = false;
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (invaders[r][c]) left = true;
  if (!left) enterGameOver();
}

void drawInvaders() {
  display.clearDisplay();
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (invaders[r][c]) {
        int x = 12 + c * 16 + invaderOffsetX;
        int y = 15 + r * 9 + invaderDrop;
        display.fillRect(x + 2, y, 8, 3, SSD1306_WHITE);
        display.drawLine(x, y + 3, x + 11, y + 3, SSD1306_WHITE);
      }
  display.fillRect(invaderPlayerX, 58, 9, 3, SSD1306_WHITE);
  for (int i = 0; i < INV_MAX_BULLETS; i++)
    if (invaderBulletActive[i])
      display.drawFastVLine(invaderBulletX[i], invaderBulletY[i], 3, SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("S:");
  display.print(invaderScore);
  display.print(" L:");
  display.print(invaderLives);
  display.display();
}

void fireAsteroid() {
  for (int i = 0; i < 3; i++) {
    if (!asteroidBulletActive[i]) {
      asteroidBulletActive[i] = true;
      asteroidBulletX[i] = asteroidShipX;
      asteroidBulletY[i] = asteroidShipY - 5;
      asteroidBulletVX[i] = 0;
      asteroidBulletVY[i] = -4;
      return;
    }
  }
}

void updateAsteroids() {
  asteroidShipX = constrain(asteroidShipX + joyXDir() * 2, 4, 123);
  asteroidShipY = constrain(asteroidShipY + joyYDir() * 2, 12, 58);
  if (buttonPressed()) fireAsteroid();

  for (int i = 0; i < 6; i++) {
    if (!asteroids[i].active) continue;
    asteroids[i].x += asteroids[i].vx;
    asteroids[i].y += asteroids[i].vy;
    if (asteroids[i].x < -8) asteroids[i].x = 136;
    if (asteroids[i].x > 136) asteroids[i].x = -8;
    if (asteroids[i].y < 8) asteroids[i].y = 58;
    if (asteroids[i].y > 60) asteroids[i].y = 8;

    int dx = asteroids[i].x - asteroidShipX;
    int dy = asteroids[i].y - asteroidShipY;
    if (dx * dx + dy * dy < 64) {
      if (--asteroidLives <= 0) {
        enterGameOver();
        return;
      }
      asteroids[i].x = random(0, 128);
      asteroids[i].y = random(12, 45);
    }
  }

  for (int b = 0; b < 3; b++) {
    if (!asteroidBulletActive[b]) continue;
    asteroidBulletX[b] += asteroidBulletVX[b];
    asteroidBulletY[b] += asteroidBulletVY[b];
    if (asteroidBulletY[b] < 8) {
      asteroidBulletActive[b] = false;
      continue;
    }
    for (int a = 0; a < 6; a++) {
      if (!asteroids[a].active) continue;
      int dx = asteroids[a].x - asteroidBulletX[b];
      int dy = asteroids[a].y - asteroidBulletY[b];
      if (dx * dx + dy * dy < 49) {
        asteroids[a].active = false;
        asteroidBulletActive[b] = false;
        asteroidScore++;
        break;
      }
    }
  }

  bool any = false;
  for (int i = 0; i < 6; i++)
    if (asteroids[i].active) any = true;
  if (!any) {
    for (int i = 0; i < 6; i++) {
      asteroids[i].active = true;
      asteroids[i].x = random(0, 128);
      asteroids[i].y = random(12, 45);
      asteroids[i].vx = random(-2, 3);
      asteroids[i].vy = random(-1, 2);
      if (!asteroids[i].vx && !asteroids[i].vy) asteroids[i].vx = 1;
    }
  }
}

void drawAsteroids() {
  display.clearDisplay();
  for (int i = 0; i < 6; i++)
    if (asteroids[i].active)
      display.drawCircle(asteroids[i].x, asteroids[i].y, asteroids[i].size, SSD1306_WHITE);
  display.drawTriangle(asteroidShipX, asteroidShipY - 5,
                       asteroidShipX - 4, asteroidShipY + 4,
                       asteroidShipX + 4, asteroidShipY + 4,
                       SSD1306_WHITE);
  for (int i = 0; i < 3; i++)
    if (asteroidBulletActive[i])
      display.fillRect(asteroidBulletX[i], asteroidBulletY[i], 2, 3, SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("S:");
  display.print(asteroidScore);
  display.print(" L:");
  display.print(asteroidLives);
  display.display();
}

void updateFlappy() {
  if (buttonPressed() || joyYDir() < 0) flappyV = -5;
  flappyV = constrain(flappyV + 1, -6, 5);
  flappyY += flappyV;
  flappyPipeX -= 2;
  if (flappyPipeX < -8) {
    flappyPipeX = 128;
    flappyGapY = random(20, 44);
    flappyScore++;
  }
  if (flappyY < 8 || flappyY > 60) {
    enterGameOver();
    return;
  }
  if (flappyPipeX < 19 && flappyPipeX + 8 > 13 &&
      (flappyY < flappyGapY - 10 || flappyY > flappyGapY + 10))
    enterGameOver();
}

void drawFlappy() {
  display.clearDisplay();
  display.fillCircle(16, flappyY, 3, SSD1306_WHITE);
  display.fillRect(flappyPipeX, 8, 8, max(0, flappyGapY - 18), SSD1306_WHITE);
  display.fillRect(flappyPipeX, flappyGapY + 10, 8, 64 - (flappyGapY + 10), SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("S:");
  display.print(flappyScore);
  display.display();
}

void updateRacing() {
  raceCarX = constrain(raceCarX + joyXDir() * 3, 42, 82);
  raceRoadOffset = (raceRoadOffset + raceSpeed) % 12;
  for (int i = 0; i < 4; i++) {
    raceObstacles[i].y += raceSpeed;
    if (raceObstacles[i].y > 64) {
      raceObstacles[i].y = -random(20, 60);
      raceObstacles[i].x = random(45, 80);
      raceScore++;
      if (raceScore % 10 == 0 && raceSpeed < 5) raceSpeed++;
    }
    if (raceObstacles[i].y > 48 && raceObstacles[i].y < 62 &&
        abs(raceObstacles[i].x - raceCarX) < 8) {
      enterGameOver();
      return;
    }
  }
}

void drawRacing() {
  display.clearDisplay();
  display.drawLine(38, 0, 38, 64, SSD1306_WHITE);
  display.drawLine(90, 0, 90, 64, SSD1306_WHITE);
  for (int y = -12 + raceRoadOffset; y < 64; y += 12)
    display.fillRect(63, y, 2, 7, SSD1306_WHITE);
  for (int i = 0; i < 4; i++)
    display.fillRect(raceObstacles[i].x - 4, raceObstacles[i].y, 8, 10, SSD1306_WHITE);
  display.fillRect(raceCarX - 4, 53, 8, 9, SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("S:");
  display.print(raceScore);
  display.print(" x");
  display.print(raceSpeed);
  display.display();
}

void updateMemory() {
  static int lx = 0, ly = 0;
  if (memoryPauseUntil) {
    if (millis() < memoryPauseUntil) return;
    if (memoryCards[memoryFirst] == memoryCards[memorySecond]) {
      memoryFound[memoryFirst] = true;
      memoryFound[memorySecond] = true;
      memoryScore++;
    }
    memoryFirst = memorySecond = -1;
    memoryPauseUntil = 0;
  }

  int x = joyXDir(), y = joyYDir();
  if (x && !lx) {
    int col = constrain(memoryCursor % 4 + x, 0, 3);
    memoryCursor = (memoryCursor / 4) * 4 + col;
  }
  if (y && !ly) {
    int row = constrain(memoryCursor / 4 + y, 0, 1);
    memoryCursor = row * 4 + memoryCursor % 4;
  }
  lx = x;
  ly = y;

  if (buttonPressed() && !memoryFound[memoryCursor]) {
    if (memoryFirst < 0) memoryFirst = memoryCursor;
    else if (memorySecond < 0 && memoryCursor != memoryFirst) {
      memorySecond = memoryCursor;
      memoryPauseUntil = millis() + 650;
    }
  }

  bool done = true;
  for (int i = 0; i < 8; i++)
    if (!memoryFound[i]) done = false;
  if (done) enterGameOver();
}

void drawMemory() {
  display.clearDisplay();
  for (int i = 0; i < 8; i++) {
    int x = 3 + (i % 4) * 31;
    int y = 10 + (i / 4) * 25;
    display.drawRect(x, y, 27, 22, SSD1306_WHITE);
    bool show = memoryFound[i] || i == memoryFirst || i == memorySecond;
    display.setCursor(x + 9, y + 7);
    display.print(show ? String(memoryCards[i] + 1) : String("?"));
    if (i == memoryCursor) display.drawRect(x - 1, y - 1, 29, 24, SSD1306_WHITE);
  }
  display.setCursor(0, 0);
  display.print("Pairs:");
  display.print(memoryScore);
  display.display();
}

void updateCoins() {
  coinPlayerX = constrain(coinPlayerX + joyXDir() * 2, 4, 123);
  coinPlayerY = constrain(coinPlayerY + joyYDir() * 2, 10, 59);
  for (int i = 0; i < 8; i++) {
    if (!coinsActive[i]) continue;
    int dx = coinsX[i] - coinPlayerX;
    int dy = coinsY[i] - coinPlayerY;
    if (dx * dx + dy * dy < 36) {
      coinsActive[i] = false;
      coinScore++;
    }
  }
  bool any = false;
  for (int i = 0; i < 8; i++)
    if (coinsActive[i]) any = true;
  if (!any) {
    for (int i = 0; i < 8; i++) {
      coinsActive[i] = true;
      coinsX[i] = random(8, 120);
      coinsY[i] = random(14, 54);
    }
  }
}

void drawCoins() {
  display.clearDisplay();
  for (int i = 0; i < 8; i++)
    if (coinsActive[i]) display.drawCircle(coinsX[i], coinsY[i], 3, SSD1306_WHITE);
  display.fillRect(coinPlayerX - 3, coinPlayerY - 3, 7, 7, SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Coins:");
  display.print(coinScore);
  display.display();
}


// =====================================================
//  GAMES 13-16
// =====================================================
bool tetCell(int p,int r,int x,int y){if(x<0||x>=4||y<0||y>=4)return false;return(tetrisShapes[p][r&3]>>(15-y*4-x))&1;}
bool tetFit(int p,int r,int px,int py){for(int y=0;y<4;y++)for(int x=0;x<4;x++)if(tetCell(p,r,x,y)){int bx=px+x,by=py+y;if(bx<0||bx>=TETRIS_W||by>=TETRIS_H||tetrisBoard[by][bx])return false;}return true;}
void tetLock(){for(int y=0;y<4;y++)for(int x=0;x<4;x++)if(tetCell(tetrisPiece,tetrisRot,x,y))tetrisBoard[tetrisY+y][tetrisX+x]=1;for(int y=TETRIS_H-1;y>=0;y--){bool full=true;for(int x=0;x<TETRIS_W;x++)if(!tetrisBoard[y][x])full=false;if(full){for(int yy=y;yy>0;yy--)for(int x=0;x<TETRIS_W;x++)tetrisBoard[yy][x]=tetrisBoard[yy-1][x];for(int x=0;x<TETRIS_W;x++)tetrisBoard[0][x]=0;tetrisLines++;tetrisScore+=10;y++;}}tetrisPiece=random(0,7);tetrisRot=0;tetrisX=3;tetrisY=0;if(!tetFit(tetrisPiece,0,3,0))enterGameOver();}
void updateTetris(){static int lx=0,ly=0;int x=joyXDir(),y=joyYDir();if(x&&!lx&&tetFit(tetrisPiece,tetrisRot,tetrisX+x,tetrisY))tetrisX+=x;if(y<0&&!ly){int r=(tetrisRot+1)&3;if(tetFit(tetrisPiece,r,tetrisX,tetrisY))tetrisRot=r;}if(buttonPressed()){while(tetFit(tetrisPiece,tetrisRot,tetrisX,tetrisY+1))tetrisY++;tetLock();return;}lx=x;ly=y;int d=max(100,650-tetrisLines*20);if(millis()-tetrisLastDrop>d){tetrisLastDrop=millis();if(tetFit(tetrisPiece,tetrisRot,tetrisX,tetrisY+1))tetrisY++;else tetLock();}}
void drawTetris(){display.clearDisplay();int ox=46,oy=8,b=4;display.drawRect(ox-1,oy-1,TETRIS_W*b+2,TETRIS_H*b+2,SSD1306_WHITE);for(int y=0;y<TETRIS_H;y++)for(int x=0;x<TETRIS_W;x++)if(tetrisBoard[y][x])display.fillRect(ox+x*b,oy+y*b,3,3,SSD1306_WHITE);for(int y=0;y<4;y++)for(int x=0;x<4;x++)if(tetCell(tetrisPiece,tetrisRot,x,y))display.fillRect(ox+(tetrisX+x)*b,oy+(tetrisY+y)*b,3,3,SSD1306_WHITE);display.setCursor(0,12);display.print("TETRIS");display.setCursor(0,25);display.print("S:");display.print(tetrisScore);display.setCursor(0,37);display.print("L:");display.print(tetrisLines);display.setCursor(0,50);display.print("BTN DROP");display.display();}

void updateLunar(){int x=joyXDir(),y=joyYDir();lunarVX=constrain(lunarVX+x*.12f,-2.0f,2.0f);if(y<0&&lunarFuel>0){lunarVY-=.28f;lunarFuel--;}lunarVY+=.10f;lunarX+=lunarVX;lunarY+=lunarVY;if(lunarX<5)lunarX=123;if(lunarX>123)lunarX=5;if(lunarY>=54){if(lunarX>=lunarPadX&&lunarX<=lunarPadX+lunarPadW&&abs(lunarVY)<1.7){lunarScore+=20;lunarY=18;lunarVY=lunarVX=0;lunarFuel=min(100,lunarFuel+35);lunarPadX=random(8,96);}else enterGameOver();}}
void drawLunar(){display.clearDisplay();display.drawLine(0,55,127,55,SSD1306_WHITE);display.fillRect(lunarPadX,53,lunarPadW,3,SSD1306_WHITE);display.drawTriangle((int)lunarX,(int)lunarY-5,(int)lunarX-4,(int)lunarY+4,(int)lunarX+4,(int)lunarY+4,SSD1306_WHITE);display.setCursor(0,0);display.print("S:");display.print(lunarScore);display.setCursor(42,0);display.print("F:");display.print(lunarFuel);display.setCursor(82,0);display.print("V:");display.print((int)(lunarVY*10));display.display();}

void updateFrogger(){static int lx=0,ly=0;int x=joyXDir(),y=joyYDir();if(x&&!lx)frogX=constrain(frogX+x*8,4,123);if(y&&!ly)frogY=constrain(frogY+y*8,8,56);lx=x;ly=y;for(int i=0;i<5;i++){frogCars[i].x+=frogCars[i].v;if(frogCars[i].v>0&&frogCars[i].x>135)frogCars[i].x=-12;if(frogCars[i].v<0&&frogCars[i].x<-12)frogCars[i].x=135;if(abs(frogCars[i].x-frogX)<8&&abs(frogCars[i].y-frogY)<6){if(--frogLives<=0)enterGameOver();else{frogX=64;frogY=56;}return;}}if(frogY<12){frogScore++;frogX=64;frogY=56;}}
void drawFrogger(){display.clearDisplay();for(int i=0;i<5;i++)display.fillRect(frogCars[i].x-5,frogCars[i].y-3,10,6,SSD1306_WHITE);display.fillRect(frogX-3,frogY-3,7,7,SSD1306_WHITE);display.setCursor(0,0);display.print("S:");display.print(frogScore);display.print(" L:");display.print(frogLives);display.display();}

void updateTron(){static unsigned long lastMove=0;int x=joyXDir(),y=joyYDir();if(x&&tronDX==0){tronDX=x;tronDY=0;}if(y&&tronDY==0){tronDX=0;tronDY=y;}if(millis()-lastMove<110)return;lastMove=millis();int nx=tronX+tronDX,ny=tronY+tronDY;if(nx<0||nx>=TRON_W||ny<0||ny>=TRON_H||tronTrail[ny][nx]){enterGameOver();return;}tronTrail[tronY][tronX]=true;tronX=nx;tronY=ny;tronScore++;int ex=tronEnemyX+tronEnemyDX,ey=tronEnemyY+tronEnemyDY;if(ex<0||ex>=TRON_W||ey<0||ey>=TRON_H||tronTrail[ey][ex]){tronEnemyDX=random(-1,2);tronEnemyDY=0;if(!tronEnemyDX)tronEnemyDY=random(-1,2);if(!tronEnemyDX&&!tronEnemyDY)tronEnemyDY=1;}else{tronTrail[tronEnemyY][tronEnemyX]=true;tronEnemyX=ex;tronEnemyY=ey;}if(tronX==tronEnemyX&&tronY==tronEnemyY)enterGameOver();}
void drawTron(){display.clearDisplay();display.drawRect(0,7,127,56,SSD1306_WHITE);for(int y=0;y<TRON_H;y++)for(int x=0;x<TRON_W;x++)if(tronTrail[y][x])display.fillRect(x*4,9+y*4,3,3,SSD1306_WHITE);display.fillRect(tronX*4,9+tronY*4,4,4,SSD1306_WHITE);display.drawRect(tronEnemyX*4,9+tronEnemyY*4,4,4,SSD1306_WHITE);display.setCursor(0,0);display.print("TRON S:");display.print(tronScore);display.display();}
void updateCurrentGame() {
  switch (currentGame) {
    case GAME_DINO: updateDino(); break;
    case GAME_BOXES: updateBoxes(); break;
    case GAME_FREE: updateFree(); break;
    case GAME_PONG: updatePong(); break;
    case GAME_SNAKE: updateSnake(); break;
    case GAME_BREAKOUT: updateBreakout(); break;
    case GAME_INVADERS: updateInvaders(); break;
    case GAME_ASTEROIDS: updateAsteroids(); break;
    case GAME_FLAPPY: updateFlappy(); break;
    case GAME_RACING: updateRacing(); break;
    case GAME_MEMORY: updateMemory(); break;
    case GAME_COINS: updateCoins(); break;
    case GAME_TETRIS: updateTetris(); break;
    case GAME_LUNAR: updateLunar(); break;
    case GAME_FROGGER: updateFrogger(); break;
    case GAME_TRON: updateTron(); break;
  }
}

void drawCurrentGame() {
  switch (currentGame) {
    case GAME_DINO: drawDino(); break;
    case GAME_BOXES: drawBoxes(); break;
    case GAME_FREE: drawFree(); break;
    case GAME_PONG: drawPong(); break;
    case GAME_SNAKE: drawSnake(); break;
    case GAME_BREAKOUT: drawBreakout(); break;
    case GAME_INVADERS: drawInvaders(); break;
    case GAME_ASTEROIDS: drawAsteroids(); break;
    case GAME_FLAPPY: drawFlappy(); break;
    case GAME_RACING: drawRacing(); break;
    case GAME_MEMORY: drawMemory(); break;
    case GAME_COINS: drawCoins(); break;
    case GAME_TETRIS: drawTetris(); break;
    case GAME_LUNAR: drawLunar(); break;
    case GAME_FROGGER: drawFrogger(); break;
    case GAME_TRON: drawTron(); break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(JOY_BTN, INPUT_PULLUP);
  analogReadResolution(12);
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 failed");
    while (true) delay(1000);
  }

  randomSeed(micros());
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(22, 25);
  display.print("POCKET ARCADE");
  display.display();
  delay(600);
  lastFrame = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < FRAME_TIME_MS) return;
  lastFrame = now;

  switch (state) {
    case STATE_MENU:
      updateMenu();
      drawMenu();
      break;
    case STATE_PLAYING:
      updateCurrentGame();
      if (state == STATE_PLAYING) drawCurrentGame();
      break;
    case STATE_GAMEOVER:
      updateGameOver();
      drawGameOver();
      break;
  }
}
