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
 *
 * V3 adds:
 *   - Scrollable game menu
 *   - Explicit function prototypes
 *   - RAM high-score framework
 *   - Non-blocking frame timing foundation
 *   - First additional game: Snake
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
  GAME_SNAKE=4
};

const int GAME_COUNT = 5;
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

// Simple RAM high-score table. Persistent storage comes later.
int highScores[GAME_COUNT] = {0, 0, 0, 0, 0};

// -------------------- Menu --------------------
int menuTop = 0;
const char* gameNames[GAME_COUNT] = {
  "1. Dino Jump",
  "2. Box Climber",
  "3. Free Walk",
  "4. Ping Pong",
  "5. Snake"
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
  return 0;
}

void recordScore() {
  int score = currentScore();
  if (score > highScores[currentGame])
    highScores[currentGame] = score;
}

void enterGameOver() {
  recordScore();
  enterGameOver();
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
      break;

    case STATE_GAMEOVER:
      updateGameOver();
      drawGameOver();
      break;
  }
}
