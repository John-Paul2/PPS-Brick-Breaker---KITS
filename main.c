#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define BRICK_ROWS 6
#define BRICK_COLS 10
#define BRICK_WIDTH 70
#define BRICK_HEIGHT 25
#define MAX_LEVELS 3

#define START_BALL_RADIUS 25.0f
#define MIN_BALL_RADIUS 5.0f
#define PADDLE_SPEED 14.0f   

typedef enum { 
    STATE_MENU, 
    STATE_PLAYING, 
    STATE_VICTORY,      
    STATE_GAME_OVER, 
    STATE_ALL_COMPLETE  
} GameState;

typedef struct {
    Rectangle rect;
    bool active;
    Color color;
    float originalY; 
} Brick;

typedef struct {
    Vector2 position;
    Vector2 speed;
    float radius;
    bool isSuper;
    float superTimer;
} Ball;

typedef struct {
    Rectangle rect;
    bool active;
    bool spawned;
} PowerUp;

typedef struct {
    Image image;
    Texture2D texture;
    int frames;
    int currentFrame;
    float timer;
    float delay;
    bool loaded;
} AnimatedBg;

Brick bricks[BRICK_ROWS][BRICK_COLS];
Rectangle paddle;
Ball ball;
PowerUp powerUpItem;
GameState currentState = STATE_MENU;

int score = 0;
int currentLevel = 0;
float waveTimer = 0.0f;
float immunityTimer = 0.0f; 

AnimatedBg bgStart, bgLevel1, bgLevel2, bgLevel3;

int levels[MAX_LEVELS][BRICK_ROWS][BRICK_COLS] = {
    { {0,0,0,0,0,0,0,0,0,0}, {0,0,1,1,1,1,1,1,0,0}, {0,0,1,1,1,1,1,1,0,0}, {0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0} },
    { {1,0,1,0,1,1,0,1,0,1}, {1,0,1,0,1,1,0,1,0,1}, {1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0} },
    { {1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1} } 
};


void DrawTextShadow(const char *text, int posX, int posY, int fontSize, Color color) {
    DrawText(text, posX + 2, posY + 2, fontSize, BLACK); // Shadow
    DrawText(text, posX, posY, fontSize, color);         // Main Text
}

void LoadGif(AnimatedBg *bg, const char *filename) {
    bg->frames = 0;
    bg->currentFrame = 0;
    bg->timer = 0.0f;
    bg->delay = 0.1f; 
    bg->loaded = false;

    if (FileExists(filename)) {
        bg->image = LoadImageAnim(filename, &bg->frames);
        bg->texture = LoadTextureFromImage(bg->image);
        SetTextureFilter(bg->texture, TEXTURE_FILTER_POINT); 
        bg->loaded = true;
    }
}

void UpdateGif(AnimatedBg *bg) {
    if (!bg->loaded) return;
    
    bg->timer += GetFrameTime();
    if (bg->timer >= bg->delay) {
        bg->currentFrame++;
        if (bg->currentFrame >= bg->frames) bg->currentFrame = 0;
        
        unsigned int nextOffset = bg->image.width * bg->image.height * 4 * bg->currentFrame;
        UpdateTexture(bg->texture, ((unsigned char *)bg->image.data) + nextOffset);
        bg->timer = 0.0f;
    }
}

void DrawGif(AnimatedBg *bg, Color tint) {
    if (bg->loaded) {
        DrawTexturePro(bg->texture, 
            (Rectangle){0, 0, (float)bg->texture.width, (float)bg->texture.height},
            (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, 
            (Vector2){0, 0}, 0.0f, tint);
    } else {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK); 
    }
}

void InitLevel(int levelIndex) {
    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            bricks[i][j].rect.x = j * (BRICK_WIDTH + 5) + 35;
            bricks[i][j].rect.y = i * (BRICK_HEIGHT + 5) + 50;
            bricks[i][j].rect.width = BRICK_WIDTH;
            bricks[i][j].rect.height = BRICK_HEIGHT;
            bricks[i][j].originalY = bricks[i][j].rect.y;
            bricks[i][j].active = (levels[levelIndex][i][j] == 1);
            
            if (levelIndex == 0) bricks[i][j].color = GREEN;       
            else if (levelIndex == 1) bricks[i][j].color = ORANGE; 
            else bricks[i][j].color = RED;                         
        }
    }
    paddle = (Rectangle){ (SCREEN_WIDTH - 120)/2, SCREEN_HEIGHT - 50, 120, 20 };
    ball.position = (Vector2){ SCREEN_WIDTH/2, SCREEN_HEIGHT/2 };
    ball.radius = START_BALL_RADIUS;
    ball.isSuper = false;
    ball.superTimer = 0.0f;
    float baseSpeed = 6.0f + (levelIndex * 1.5f);
    ball.speed = (Vector2){ baseSpeed, -baseSpeed };
    powerUpItem.active = false;
    powerUpItem.spawned = (levelIndex == 0); 
    immunityTimer = (levelIndex >= 1) ? 5.0f : 0.0f;
}

void StartGameAtLevel(int level) {
    score = 0;
    currentLevel = level;
    waveTimer = 0.0f;
    InitLevel(currentLevel);
    currentState = STATE_PLAYING;
}

bool AllBricksCleared() {
    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            if (bricks[i][j].active) return false;
        }
    }
    return true;
}

void SpawnPowerUp(Vector2 position) {
    if (!powerUpItem.spawned && !powerUpItem.active) {
        if (GetRandomValue(0, 100) < 20) { 
            powerUpItem.rect = (Rectangle){ position.x + 25, position.y, 20, 20 };
            powerUpItem.active = true;
            powerUpItem.spawned = true; 
        }
    }
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Brick Breaker - Ultimate Edition");
    SetExitKey(KEY_NULL); 
    SetTargetFPS(60);

    LoadGif(&bgStart, "assets/start_bg.gif");
    LoadGif(&bgLevel1, "assets/level1_bg.gif");
    LoadGif(&bgLevel2, "assets/level2_bg.gif");
    LoadGif(&bgLevel3, "assets/level3_bg.gif");

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (currentState == STATE_MENU) UpdateGif(&bgStart);
        else if (currentState == STATE_PLAYING) {
            if (currentLevel == 0) UpdateGif(&bgLevel1);
            if (currentLevel == 1) UpdateGif(&bgLevel2);
            if (currentLevel == 2) UpdateGif(&bgLevel3);
        }
        
        switch (currentState) {
            case STATE_MENU:
                if (IsKeyPressed(KEY_ONE)) StartGameAtLevel(0);
                if (IsKeyPressed(KEY_TWO)) StartGameAtLevel(1);
                if (IsKeyPressed(KEY_THREE)) StartGameAtLevel(2);
                if (IsKeyPressed(KEY_ENTER)) StartGameAtLevel(0);
                break;

            case STATE_PLAYING:
            {
                if (immunityTimer > 0) immunityTimer -= dt;
                if (ball.isSuper) {
                    ball.superTimer -= dt;
                    if (ball.superTimer <= 0) ball.isSuper = false;
                }

                if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) paddle.x -= PADDLE_SPEED;
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) paddle.x += PADDLE_SPEED;
                if (paddle.x < 0) paddle.x = 0;
                if (paddle.x > SCREEN_WIDTH - paddle.width) paddle.x = SCREEN_WIDTH - paddle.width;

                if (powerUpItem.active) {
                    powerUpItem.rect.y += 4.0f;
                    if (CheckCollisionRecs(powerUpItem.rect, paddle)) {
                        powerUpItem.active = false;
                        ball.isSuper = true;
                        ball.superTimer = 5.0f; 
                    }
                    if (powerUpItem.rect.y > SCREEN_HEIGHT) powerUpItem.active = false;
                }

                if (currentLevel >= 1) {
                    waveTimer += 0.08f; 
                    for (int i = 0; i < BRICK_ROWS; i++) {
                        for (int j = 0; j < BRICK_COLS; j++) {
                            if (bricks[i][j].active) {
                                float waveHeight = (currentLevel == 1) ? 5.0f : 15.0f;
                                bricks[i][j].rect.y = bricks[i][j].originalY + sinf(waveTimer + j) * waveHeight;
                            }
                        }
                    }
                }

                ball.position.x += ball.speed.x;
                ball.position.y += ball.speed.y;

                if (ball.position.x >= (SCREEN_WIDTH - ball.radius) || ball.position.x <= ball.radius) ball.speed.x *= -1.0f;
                if (ball.position.y <= ball.radius) ball.speed.y *= -1.0f;

                if (ball.position.y >= SCREEN_HEIGHT) {
                    if (immunityTimer > 0) {
                        ball.speed.y *= -1.0f;
                        ball.position.y = SCREEN_HEIGHT - ball.radius - 1;
                    } else {
                        currentState = STATE_GAME_OVER;
                    }
                }

                if (CheckCollisionCircleRec(ball.position, ball.radius, paddle)) {
                    ball.speed.y *= -1.0f;
                    ball.position.y = paddle.y - ball.radius - 1;
                }

                for (int i = 0; i < BRICK_ROWS; i++) {
                    for (int j = 0; j < BRICK_COLS; j++) {
                        if (bricks[i][j].active) {
                            if (CheckCollisionCircleRec(ball.position, ball.radius, bricks[i][j].rect)) {
                                bricks[i][j].active = false;
                                score += 100;
                                if (!ball.isSuper) ball.speed.y *= -1.0f; 
                                SpawnPowerUp((Vector2){bricks[i][j].rect.x, bricks[i][j].rect.y});
                                float shrinkAmount = 0.0f;
                                if (currentLevel == 0) shrinkAmount = 0.2f;
                                if (currentLevel == 1) shrinkAmount = 1.0f;
                                if (currentLevel == 2) shrinkAmount = 1.2f; 
                                if (ball.radius > MIN_BALL_RADIUS) ball.radius -= shrinkAmount;
                            }
                        }
                    }
                }

                if (AllBricksCleared()) {
                    currentLevel++;
                    if (currentLevel >= MAX_LEVELS) currentState = STATE_ALL_COMPLETE;
                    else currentState = STATE_VICTORY; 
                }
            } break;

            case STATE_VICTORY: 
                if (IsKeyPressed(KEY_ENTER)) { InitLevel(currentLevel); currentState = STATE_PLAYING; }
                break;
            case STATE_GAME_OVER: 
                if (IsKeyPressed(KEY_ENTER)) currentState = STATE_MENU; 
                break;
            case STATE_ALL_COMPLETE: 
                if (IsKeyPressed(KEY_ENTER)) currentState = STATE_MENU;
                break;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        if (currentState == STATE_MENU) {
            DrawGif(&bgStart, WHITE);
            DrawRectangle(0, 120, SCREEN_WIDTH, 400, (Color){0,0,0,180}); 

            DrawTextShadow("NEON BRICK BREAKER", SCREEN_WIDTH/2 - 220, 150, 40, WHITE);
            
            DrawTextShadow("SELECT LEVEL", SCREEN_WIDTH/2 - 100, 230, 20, GRAY);
            DrawTextShadow("[1] FOREST (Easy)", SCREEN_WIDTH/2 - 120, 260, 30, GREEN);
            DrawTextShadow("[2] CITY (Medium)", SCREEN_WIDTH/2 - 120, 300, 30, ORANGE);
            DrawTextShadow("[3] STORM (Hard)", SCREEN_WIDTH/2 - 120, 340, 30, RED);
            
            DrawTextShadow("Press [ENTER] to Start", SCREEN_WIDTH/2 - 180, 450, 30, YELLOW);
            
            if (!bgStart.loaded) DrawText("Error: 'assets/start_bg.gif' missing!", 10, 10, 20, RED);
        }
        else if (currentState == STATE_PLAYING) {
            if (currentLevel == 0) DrawGif(&bgLevel1, (Color){200, 200, 200, 255}); // Slight dim
            else if (currentLevel == 1) DrawGif(&bgLevel2, (Color){200, 200, 200, 255});
            else if (currentLevel == 2) DrawGif(&bgLevel3, (Color){200, 200, 200, 255});

            for (int i = 0; i < BRICK_ROWS; i++) {
                for (int j = 0; j < BRICK_COLS; j++) {
                    if (bricks[i][j].active) {
                        DrawRectangleRec(bricks[i][j].rect, bricks[i][j].color);
                        DrawRectangleLinesEx(bricks[i][j].rect, 2, BLACK);
                    }
                }
            }
            DrawRectangleRec(paddle, BLUE);
            DrawRectangleLinesEx(paddle, 2, WHITE);
            
            Color ballColor = WHITE;
            if (ball.isSuper) ballColor = ORANGE; 
            else if (immunityTimer > 0) ballColor = SKYBLUE; 
            else if (ball.radius < 8) ballColor = RED;
            
            DrawCircleV(ball.position, ball.radius, ballColor);
            DrawCircleLines(ball.position.x, ball.position.y, ball.radius, BLACK); // Outline

            if (powerUpItem.active) {
                DrawRectangleRec(powerUpItem.rect, PURPLE);
                DrawText("P", (int)powerUpItem.rect.x + 5, (int)powerUpItem.rect.y + 2, 20, WHITE);
            }

            DrawTextShadow(TextFormat("Score: %d", score), 10, 10, 25, WHITE);
            
            if (immunityTimer > 0) {
                 DrawTextShadow("IMMUNITY!", SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 + 50, 20, SKYBLUE);
                 DrawTextShadow(TextFormat("%.1f", immunityTimer), SCREEN_WIDTH/2 - 20, SCREEN_HEIGHT/2 + 80, 40, SKYBLUE);
            }
            if (ball.isSuper) DrawTextShadow("SUPER BALL!", SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 + 120, 20, ORANGE);
        }
        else if (currentState == STATE_GAME_OVER || currentState == STATE_ALL_COMPLETE) {
            ClearBackground(BLACK);
            DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){40,0,0,255}, BLACK);
            
            DrawRectangle(SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 150, 400, 300, (Color){20, 20, 20, 230});
            DrawRectangleLines(SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 150, 400, 300, WHITE);
            
            const char* title = (currentState == STATE_ALL_COMPLETE) ? "VICTORY!" : "GAME OVER";
            Color titleColor = (currentState == STATE_ALL_COMPLETE) ? GOLD : RED;
            
            DrawTextShadow(title, SCREEN_WIDTH/2 - MeasureText(title, 50)/2, SCREEN_HEIGHT/2 - 100, 50, titleColor);
            DrawTextShadow("FINAL SCORE", SCREEN_WIDTH/2 - MeasureText("FINAL SCORE", 30)/2, SCREEN_HEIGHT/2 - 20, 30, LIGHTGRAY);
            DrawTextShadow(TextFormat("%d", score), SCREEN_WIDTH/2 - MeasureText(TextFormat("%d", score), 60)/2, SCREEN_HEIGHT/2 + 20, 60, WHITE);
            DrawTextShadow("Press [ENTER] to Menu", SCREEN_WIDTH/2 - MeasureText("Press [ENTER] to Menu", 20)/2, SCREEN_HEIGHT/2 + 100, 20, YELLOW);
        }
        else if (currentState == STATE_VICTORY) {
            ClearBackground(DARKBLUE);
            DrawTextShadow("LEVEL CLEARED!", SCREEN_WIDTH/2 - 150, 250, 40, WHITE);
            DrawTextShadow("Press [ENTER] for Next Level", SCREEN_WIDTH/2 - 160, 320, 20, YELLOW);
        }

        EndDrawing();
    }

    if(bgStart.loaded) UnloadImage(bgStart.image); UnloadTexture(bgStart.texture);
    if(bgLevel1.loaded) UnloadImage(bgLevel1.image); UnloadTexture(bgLevel1.texture);
    if(bgLevel2.loaded) UnloadImage(bgLevel2.image); UnloadTexture(bgLevel2.texture);
    if(bgLevel3.loaded) UnloadImage(bgLevel3.image); UnloadTexture(bgLevel3.texture);
    
    CloseWindow();
    return 0;
}