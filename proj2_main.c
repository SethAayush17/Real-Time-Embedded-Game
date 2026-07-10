/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

/* Standard Includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/grlib/grlib.h>
#include <stdlib.h>


#include "HAL/LcdDriver/Crystalfontz128x128_ST7735.h"  // Need these drivers to use the LCD!

/* HAL and Application includes */
#include <Application.h>
#include <HAL/HAL.h>
#include <HAL/Timer.h>

// Checks if the new score is good enough to make the top 5 list
void Application_updateHighScores(Application* app_p, int newScore) {

    int insertPos = -1;
    int i;


    for (i = 0; i < 5; i++) {
        if (newScore > app_p->highScores[i]) {
            insertPos = i;
            break;
        }
    }


    if (insertPos != -1) {
        int j;
        for (j = 4; j > insertPos; j--) {
            app_p->highScores[j] = app_p->highScores[j - 1];
        }
        app_p->highScores[insertPos] = newScore;
    }
}

// Function to initialize high scores;
// This was made as a function in order to not add too many lines to the application construct function
void Application_initHighScores(Application* app_p) {
    int i;
    for (i = 0; i < 5; i++) {
        app_p->highScores[i] = 0;  // Initialize all scores to 0
    }
}



// Non-blocking check. Whenever Launchpad S1 is pressed, LED1 turns on.
static void InitNonBlockingLED() {
  GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
  GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1);
}

// Non-blocking check. Whenever Launchpad S1 is pressed, LED1 turns on.
static void PollNonBlockingLED() {
  GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
  if (GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN1) == 0) {
    GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
  }
}

// Initializes the graphics context for display operations
void initializeGraphics(Graphics_Context *g_sContext_p);


int main(void) {
  // Stop Watchdog Timer - THIS SHOULD ALWAYS BE THE FIRST LINE OF YOUR MAIN
  WDT_A_holdTimer();

  // Initialize the system clock and background hardware timer, used to enable
  // software timers to time their measurements properly.
  InitSystemTiming();

  // Initialize the main Application object and HAL object
  HAL hal = HAL_construct();
  Application app = Application_construct();

  // Do not remove this line. This is your non-blocking check.
  InitNonBlockingLED();

  // Main super-loop! In a polling architecture, this function should call
  // your main FSM function over and over.
  while (true) {
    // Do not remove this line. This is your non-blocking check.
    PollNonBlockingLED();
    HAL_refresh(&hal);
    Application_loop(&app, &hal);
  }
}

Application Application_construct() {
        Application app;

        Application_initHighScores(&app);

        // ========== Screen State Initialization ==========
        app.currentScreen = TITLE_SCREEN;               // Start at title screen
        app.titleScreenDrawn = false;                   // Title screen not yet drawn
        app.titleTimer = SWTimer_construct(3000);       // Create 3-second timer for title screen
        SWTimer_start(&app.titleTimer);                 // Start the title timer
        app.menuScreenDrawn = false;                    // Menu screen not yet drawn
        app.menuSelection = 0;                          // Default menu selection (first option)
        app.instructionsScreenDrawn = false;            // Instructions screen not yet drawn
        app.HighScoresScreenDrawn = false;              // High scores screen not yet drawn
        app.PlayScreenDrawn = false;                    // Play screen not yet initialized
        app.gameOverScreenDrawn = false;

        // ========== Player State Initialization ==========
        app.playerX = 56;                               // Center player horizontally
        app.playerY = 56;                               // Center player vertically
        app.playerHP = 25;                             // Start with full health
        app.moveFrameCounter = 0;                       // Reset movement timer
        app.shootButtonWasPressed = false;              // Button not pressed initially

        // Initialize all player bullets as inactive
        int i;
        for (i = 0; i < MAX_BULLETS; i++) {
            app.bullets[i].active = false;              // Bullet not active
            app.bullets[i].x = 0;                       // Reset X position
            app.bullets[i].y = 0;                       // Reset Y position
            app.bullets[i].frameCounter = 0;            // Reset movement timer
        }

        // ========== Enemy State Initialization ==========
        app.enemyHP = 25;                              // Start enemy with full health
        app.enemyX = 59;                                // Start enemy near center
        app.enemyY = 25;                                // Position enemy just below game title
        app.enemyDirection = 1;                         // Start moving right
        app.enemyFrameCounter = 0;                      // Reset movement timer

        // Initialize all enemy bullets as inactive
        for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
            app.enemyBullets[i].active = false;         // Bullet not active
            app.enemyBullets[i].x = 0;                  // Reset X position
            app.enemyBullets[i].y = 0;                  // Reset Y position
            app.enemyBullets[i].frameCounter = 0;       // Reset movement timer
            app.enemyBullets[i].directionX = 0;         // No horizontal movement initially
            app.enemyBullets[i].directionY = 1;         // Default to downward movement
            app.enemyBullets[i].isHorizontal = false;   // Default to vertical bullets
        }

        // ========== Attack Pattern Initialization ==========
        app.currentAttackPattern = 0;                   // Start with pattern 0 (vertical raindrops)
        app.patternFrameCounter = 0;                    // Reset pattern spawn timer
        app.attackCooldownCounter = 0;                  // Reset cooldown timer
        app.patternActive = false;                      // No pattern active initially

        // ========== Game Timer Initialization ==========
        app.gameTimer = 0;                              // Start timer at 0 seconds
        app.timerFrameCounter = 0;                      // Reset frame counter

        // ========== Random Number Generator ==========
        srand(1);                                    //Generator for random numbers

        return app;
    }


void Application_loop(Application* app_p, HAL* hal_p) {
    switch (app_p->currentScreen) {
       case TITLE_SCREEN:
         Application_handleTitleScreen(app_p, hal_p);
         break;

       case MENU_SCREEN:
           Application_handleMenuScreen(app_p, hal_p);
           break;
       case INSTRUCTIONS_SCREEN:
           Application_handleInstructionsScreen(app_p,hal_p);
           break;
       case HIGH_SCORES_SCREEN:
           Application_handleHighScoresScreen(app_p,hal_p);
           break;
       case PLAY_SCREEN:
           Application_handlePlayScreen(app_p,hal_p);
          break;
  }

}



void initializeGraphics(Graphics_Context *g_sContext_p) {
  // Initialize the LCD
  Crystalfontz128x128_Init();
  Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);

  // Initialize context
  Graphics_initContext(g_sContext_p, &g_sCrystalfontz128x128, &g_sCrystalfontz128x128_funcs);

  // Set colors and fonts
  Graphics_setForegroundColor(g_sContext_p, GRAPHICS_COLOR_WHITE);
  Graphics_setBackgroundColor(g_sContext_p, GRAPHICS_COLOR_BLACK);
  Graphics_setFont(g_sContext_p, &g_sFontFixed6x8);

  // Clear the screen
  Graphics_clearDisplay(g_sContext_p);
}



// Shows game information for 3 seconds before transitioning to menu.
void Application_handleTitleScreen(Application* app_p, HAL* hal_p){


    // Check if we haven't drawn the title screen yet this time
    if (!app_p->titleScreenDrawn) {

        Graphics_Context g_sContext;
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);

        Graphics_drawString(&g_sContext, (int8_t *)"ECE2564", -1, 10, 10, true);

        Graphics_drawString(&g_sContext, (int8_t *)"Project 2", -1, 10, 25, true);

        Graphics_drawString(&g_sContext, (int8_t *)"----------------", -1, 10, 35, true);

        Graphics_drawString(&g_sContext, (int8_t *)"Touhou", -1, 10, 60, true);

        app_p->titleScreenDrawn = true;
    }

    // Check if 3 seconds (3000ms) have passed since timer started
    if (SWTimer_expired(&app_p->titleTimer)) {
        app_p->currentScreen = MENU_SCREEN;
        app_p->titleScreenDrawn = false;
    }
}


//Handles the menu screen where players select Play, Instructions, or High Scores.
// Uses buttons to navigate up/down and joystick button to confirm selection.

void Application_handleMenuScreen(Application* app_p, HAL* hal_p){
    static Graphics_Context g_sContext;

    // Initial Menu Screen Draw
    if (!app_p->menuScreenDrawn) {
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_CYAN);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_drawString(&g_sContext, (int8_t *)"MENU", -1, 5, 5, true);
        app_p->menuScreenDrawn = true;
    }

    static int lastSelection = -1;

    // Only redraw if the user moved to a different menu item
    if (lastSelection != app_p->menuSelection) {
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_CYAN);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);


        if (app_p->menuSelection == 0) {
            Graphics_drawString(&g_sContext, (int8_t *)"> Play", -1, 5, 40, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  Instructions", -1, 5, 55, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  High Scores", -1, 5, 70, true);
        } else if (app_p->menuSelection == 1) {
            Graphics_drawString(&g_sContext, (int8_t *)"  Play", -1, 5, 40, true);
            Graphics_drawString(&g_sContext, (int8_t *)"> Instructions", -1, 5, 55, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  High Scores", -1, 5, 70, true);
        } else {
            Graphics_drawString(&g_sContext, (int8_t *)"  Play", -1, 5, 40, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  Instructions", -1, 5, 55, true);
            Graphics_drawString(&g_sContext, (int8_t *)"> High Scores", -1, 5, 70, true);
        }

        lastSelection = app_p->menuSelection;
    }



    if (Button_isTapped(&hal_p->boosterpackS1)) {
        app_p->menuSelection--;
        if (app_p->menuSelection < 0) {
            app_p->menuSelection = 2;
        }
    }


    if (Button_isTapped(&hal_p->boosterpackS2)) {
        app_p->menuSelection++;
        if (app_p->menuSelection > 2) {
            app_p->menuSelection = 0;
        }
    }


    if (Button_isTapped(&hal_p->boosterpackJS)) {

        app_p->menuScreenDrawn = false;
        lastSelection = -1;

        switch(app_p->menuSelection) {
            case 0:
                app_p->currentScreen = PLAY_SCREEN;
                break;
            case 1:
                app_p->currentScreen = INSTRUCTIONS_SCREEN;
                break;
            case 2:
                app_p->currentScreen = HIGH_SCORES_SCREEN;
                break;
        }
    }
}


// Displays game instructions and controls.

void Application_handleInstructionsScreen(Application* app_p, HAL* hal_p){

    //Draw Instructions Screen
    if (!app_p->instructionsScreenDrawn) {
        Graphics_Context g_sContext;
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);


        Graphics_drawString(&g_sContext, (int8_t *)"Instructions", -1, 20, 5, true);


        Graphics_drawLine(&g_sContext, 5, 15, 123, 15);

        Graphics_drawString(&g_sContext, (int8_t *)"Move the character", -1, 5, 20, true);
        Graphics_drawString(&g_sContext, (int8_t *)"to avoid being hit by", -1, 5, 30, true);
        Graphics_drawString(&g_sContext, (int8_t *)"the bullets.", -1, 5, 40, true);
        Graphics_drawString(&g_sContext, (int8_t *)"Press BB1 to fire at", -1, 5, 50, true);
        Graphics_drawString(&g_sContext, (int8_t *)"the enemy.", -1, 5, 60, true);
        Graphics_drawString(&g_sContext, (int8_t *)"First one to run out", -1, 5, 70, true);
        Graphics_drawString(&g_sContext, (int8_t *)"of health loses.", -1, 5, 80, true);
        Graphics_drawString(&g_sContext, (int8_t *)"Press BB2 to return", -1, 5, 110, true);

        app_p->instructionsScreenDrawn = true;
    }

    // Logic for returning to menu

    if (Button_isTapped(&hal_p->boosterpackS2)) {
        app_p->currentScreen = MENU_SCREEN;
        app_p->instructionsScreenDrawn = false;
        app_p->menuScreenDrawn = false;
    }
}


//Displays the top 5 high scores.

void Application_handleHighScoresScreen(Application* app_p, HAL* hal_p){

    // Draw High Scores Screen
    if (!app_p->HighScoresScreenDrawn) {
        Graphics_Context g_sContext;
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);


        Graphics_drawString(&g_sContext, (int8_t *)"High Scores", -1, 25, 5, true);
        Graphics_drawLine(&g_sContext, 5, 15, 123, 15);


        char scoreText[20];
        int i;
        for (i = 0; i < 5; i++) {
            sprintf(scoreText, "%d. %d", i + 1, app_p->highScores[i]);
            Graphics_drawString(&g_sContext, (int8_t *)scoreText, -1, 40, 25 + (i * 15), true);
        }


        Graphics_drawString(&g_sContext, (int8_t *)"Press BB2 to return", -1, 5, 110, true);
        app_p->HighScoresScreenDrawn = true;
    }

    // Return to menu

    if (Button_isTapped(&hal_p->boosterpackS2)) {
        app_p->currentScreen = MENU_SCREEN;
        app_p->HighScoresScreenDrawn = false;
        app_p->menuScreenDrawn = false;
    }
}

// Helper function to draw initial game screen
void drawInitialGameScreen(Graphics_Context* g_sContext, Application* app_p,
                          int hpCircleX, int hpCircleY, int hpCircleRadius,
                          int enemyHpCircleX, int enemyHpCircleY, int enemyHpCircleRadius,
                          int playerSize, int enemySize) {
    initializeGraphics(g_sContext);
    Graphics_clearDisplay(g_sContext);

    // Draw timer
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
    char timerText[10];
    sprintf(timerText, "T:%d", app_p->gameTimer);
    Graphics_drawString(g_sContext, (int8_t *)timerText, -1, 50, 5, true);
    Graphics_drawString(g_sContext, (int8_t *)"Game", -1, 5, 5, true);

    // Draw player HP circle
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
    Graphics_drawCircle(g_sContext, hpCircleX, hpCircleY, hpCircleRadius);
    char hpText[4];
    sprintf(hpText, "%d", app_p->playerHP);
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_drawString(g_sContext, (int8_t *)hpText, -1, hpCircleX - 9, hpCircleY - 4, true);

    // Draw enemy HP circle
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
    Graphics_drawCircle(g_sContext, enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius);
    char enemyHpText[4];
    sprintf(enemyHpText, "%d", app_p->enemyHP);
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_drawString(g_sContext, (int8_t *)enemyHpText, -1, enemyHpCircleX - 9, enemyHpCircleY - 4, true);

    // Draw player and enemy
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
    Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){
        app_p->playerX, app_p->playerY,
        app_p->playerX + playerSize, app_p->playerY + playerSize
    });

    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
    Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){
        app_p->enemyX, app_p->enemyY,
        app_p->enemyX + enemySize, app_p->enemyY + enemySize
    });
}

// Helper function to show game over screen

bool handleGameOver(Graphics_Context* g_sContext, Application* app_p, HAL* hal_p) {
    if (app_p->playerHP <= 0 || app_p->enemyHP <= 0) {
        if (!app_p->gameOverScreenDrawn) {
            int finalScore = app_p->gameTimer + app_p->playerHP;
            Application_updateHighScores(app_p, finalScore);

            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
            Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){0, 92, 128, 128});

            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
            Graphics_setBackgroundColor(g_sContext, GRAPHICS_COLOR_BLACK);

            if (app_p->enemyHP <= 0) {
                Graphics_drawString(g_sContext, (int8_t *)"WIN!", -1, 48, 95, true);
            } else {
                Graphics_drawString(g_sContext, (int8_t *)"LOSE", -1, 48, 95, true);
            }

            char scoreText[15];
            sprintf(scoreText, "S:%d", finalScore);
            Graphics_drawString(g_sContext, (int8_t *)scoreText, -1, 45, 106, true);
            Graphics_drawString(g_sContext, (int8_t *)"Press JSB", -1, 35, 117, true);

            app_p->gameOverScreenDrawn = true;
        }

        if (Button_isPressed(&hal_p->boosterpackJS)) {
            app_p->currentScreen = TITLE_SCREEN;
            app_p->titleScreenDrawn = false;
            app_p->playerHP = 100;
            app_p->enemyHP = 100;
            app_p->gameTimer = 0;
            app_p->PlayScreenDrawn = false;
            app_p->gameOverScreenDrawn = false;
        }

        return true;
    }
    return false;
}

// Helper function to handle player movement

void handlePlayerMovement(Application* app_p, Graphics_Context* g_sContext,
                         int playerSize, int screenWidth, int screenHeight,
                         int topBoundary, int hpCircleArea, int moveSpeed,
                         int moveUpdateFrequency, int hpCircleX, int hpCircleY, int hpCircleRadius) {
    unsigned vx = ADC14_getResult(ADC_MEM0);
    unsigned vy = ADC14_getResult(ADC_MEM1);

    const int CENTER = 8192;
    const int DEAD_ZONE = 1000;

    int oldX = app_p->playerX;
    int oldY = app_p->playerY;

    app_p->moveFrameCounter++;

    // Only update position every moveUpdateFrequency frames
    if (app_p->moveFrameCounter >= moveUpdateFrequency) {
        app_p->moveFrameCounter = 0;

        if (vx > CENTER + DEAD_ZONE) app_p->playerX += moveSpeed;
        if (vx < CENTER - DEAD_ZONE) app_p->playerX -= moveSpeed;
        if (vy < CENTER - DEAD_ZONE) app_p->playerY += moveSpeed;
        if (vy > CENTER + DEAD_ZONE) app_p->playerY -= moveSpeed;
    }

    // Enforce boundaries
    if (app_p->playerX < 0) app_p->playerX = 0;
    if (app_p->playerX > screenWidth - playerSize) app_p->playerX = screenWidth - playerSize;
    if (app_p->playerY < topBoundary) app_p->playerY = topBoundary;
    if (app_p->playerY > screenHeight - playerSize) app_p->playerY = screenHeight - playerSize;

    // Keep player from overlapping HP circle
    if (app_p->playerX < hpCircleArea && app_p->playerY > screenHeight - hpCircleArea) {
        if (oldX >= hpCircleArea) {
            app_p->playerX = hpCircleArea;
        } else if (oldY <= screenHeight - hpCircleArea) {
            app_p->playerY = screenHeight - hpCircleArea;
        } else {
            int distRight = hpCircleArea - app_p->playerX;
            int distUp = (screenHeight - hpCircleArea) - app_p->playerY;
            if (distRight < distUp) {
                app_p->playerX = hpCircleArea;
            } else {
                app_p->playerY = screenHeight - hpCircleArea;
            }
        }
    }

    // Redraw if position changed
    if (oldX != app_p->playerX || oldY != app_p->playerY) {
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){oldX, oldY, oldX + playerSize, oldY + playerSize});

        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){app_p->playerX, app_p->playerY,
        app_p->playerX + playerSize, app_p->playerY + playerSize});

        // Redraw HP circle if player was near it
        if (oldX < hpCircleArea + playerSize && oldY > screenHeight - hpCircleArea - playerSize) {
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
            Graphics_drawCircle(g_sContext, hpCircleX, hpCircleY, hpCircleRadius);
            char hpText[4];
            sprintf(hpText, "%d", app_p->playerHP);
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
            Graphics_drawString(g_sContext, (int8_t *)hpText, -1, hpCircleX - 9, hpCircleY - 4, true);
        }
    }
}

// Helper function to update game timer

void updateGameTimer(Application* app_p, Graphics_Context* g_sContext) {
    app_p->timerFrameCounter++;
    if (app_p->timerFrameCounter >= 240) {
        app_p->timerFrameCounter = 0;
        app_p->gameTimer++;

        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){50, 5, 90, 13});

        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
        char timerText[10];
        sprintf(timerText, "T:%d", app_p->gameTimer);
        Graphics_drawString(g_sContext, (int8_t *)timerText, -1, 50, 5, true);
    }
}

// Helper function to update enemy position

void updateEnemyPosition(Application* app_p, Graphics_Context* g_sContext,
                        int enemySize, int enemySpeed, int enemyUpdateFrequency,
                        int screenWidth, int enemyHpCircleArea) {
    int oldEnemyX = app_p->enemyX;
    app_p->enemyFrameCounter++;

    if (app_p->enemyFrameCounter >= enemyUpdateFrequency) {
        app_p->enemyFrameCounter = 0;
        app_p->enemyX += enemySpeed * app_p->enemyDirection;

        if (app_p->enemyX <= 0) {
            app_p->enemyX = 0;
            app_p->enemyDirection = 1;
        }
        if (app_p->enemyX >= screenWidth - enemyHpCircleArea) {
            app_p->enemyX = screenWidth - enemyHpCircleArea;
            app_p->enemyDirection = -1;
        }
    }

    if (oldEnemyX != app_p->enemyX) {
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){
        oldEnemyX, app_p->enemyY, oldEnemyX + enemySize, app_p->enemyY + enemySize
        });

        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){
        app_p->enemyX, app_p->enemyY, app_p->enemyX + enemySize, app_p->enemyY + enemySize
        });
    }
}

// Helper function for pattern 1 (vertical rain)

void spawnPattern1(Application* app_p, int topBoundary) {
    if (app_p->patternFrameCounter % 40 == 0) {
        int bulletIndex = app_p->patternFrameCounter / 40;
        if (bulletIndex < 8) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    app_p->enemyBullets[i].x = 10 + (bulletIndex * 15);
                    app_p->enemyBullets[i].y = topBoundary;
                    app_p->enemyBullets[i].directionX = 0;
                    app_p->enemyBullets[i].directionY = 1;
                    app_p->enemyBullets[i].isHorizontal = false;
                    app_p->enemyBullets[i].active = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// Helper function for pattern 2 (horizontal rain from left)

void spawnPattern2(Application* app_p, int topBoundary) {
    if (app_p->patternFrameCounter % 25 == 0) {
        int bulletIndex = app_p->patternFrameCounter / 25;
        if (bulletIndex < 8) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    app_p->enemyBullets[i].x = 0;
                    app_p->enemyBullets[i].y = topBoundary + (bulletIndex * 12);
                    app_p->enemyBullets[i].directionX = 1;
                    app_p->enemyBullets[i].directionY = 0;
                    app_p->enemyBullets[i].isHorizontal = true;
                    app_p->enemyBullets[i].active = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// Helper function for pattern 3 (diagonal from corners)

void spawnPattern3(Application* app_p, int topBoundary, int screenWidth, int patternSpawnInterval) {
    if (app_p->patternFrameCounter % patternSpawnInterval == 0) {
        int bulletIndex = app_p->patternFrameCounter / patternSpawnInterval;
        if (bulletIndex < 16) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    if (bulletIndex % 2 == 0) {
                        app_p->enemyBullets[i].x = 0;
                        app_p->enemyBullets[i].y = topBoundary;
                        app_p->enemyBullets[i].directionX = 1;
                        app_p->enemyBullets[i].directionY = 1;
                    } else {
                        app_p->enemyBullets[i].x = screenWidth - 1;
                        app_p->enemyBullets[i].y = topBoundary;
                        app_p->enemyBullets[i].directionX = -1;
                        app_p->enemyBullets[i].directionY = 1;
                    }
                    app_p->enemyBullets[i].isHorizontal = false;
                    app_p->enemyBullets[i].active = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// Helper function for pattern 4 (alternating horizontal)

void spawnPattern4(Application* app_p, int topBoundary, int screenWidth, int patternSpawnInterval) {
    if (app_p->patternFrameCounter % patternSpawnInterval == 0) {
        int bulletIndex = app_p->patternFrameCounter / patternSpawnInterval;
        if (bulletIndex < 12) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    if (bulletIndex % 2 == 0) {
                        app_p->enemyBullets[i].x = 0;
                        app_p->enemyBullets[i].y = topBoundary + (bulletIndex * 8);
                        app_p->enemyBullets[i].directionX = 1;
                        app_p->enemyBullets[i].directionY = 0;
                    } else {
                        app_p->enemyBullets[i].x = screenWidth - 1;
                        app_p->enemyBullets[i].y = topBoundary + (bulletIndex * 8);
                        app_p->enemyBullets[i].directionX = -1;
                        app_p->enemyBullets[i].directionY = 0;
                    }
                    app_p->enemyBullets[i].isHorizontal = true;
                    app_p->enemyBullets[i].active = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// Helper function to manage enemy attack patterns

void manageEnemyAttackPatterns(Application* app_p, int topBoundary, int screenWidth,
                               int patternSpawnInterval, int attackCooldown) {
    // Check if all bullets are done
    bool allEnemyBulletsInactive = true;
    int j;
    for (j = 0; j < MAX_ENEMY_BULLETS; j++) {
        if (app_p->enemyBullets[j].active) {
            allEnemyBulletsInactive = false;
            break;
        }
    }

    // Start new pattern if ready
    if (!app_p->patternActive && allEnemyBulletsInactive) {
        app_p->attackCooldownCounter++;
        if (app_p->attackCooldownCounter >= attackCooldown) {
            app_p->patternActive = true;
            app_p->patternFrameCounter = 0;
            app_p->attackCooldownCounter = 0;
            app_p->currentAttackPattern = (app_p->currentAttackPattern + 1) % 4;
        }
    }

    // Execute current pattern
    if (app_p->patternActive) {
        app_p->patternFrameCounter++;

        if (app_p->currentAttackPattern == 0) {
            spawnPattern1(app_p, topBoundary);
        } else if (app_p->currentAttackPattern == 1) {
            spawnPattern2(app_p, topBoundary);
        } else if (app_p->currentAttackPattern == 2) {
            spawnPattern3(app_p, topBoundary, screenWidth, patternSpawnInterval);
        } else if (app_p->currentAttackPattern == 3) {
            spawnPattern4(app_p, topBoundary, screenWidth, patternSpawnInterval);
        }
    }
}

// Helper function to update single enemy bullet

void updateSingleEnemyBullet(Application* app_p, Graphics_Context* g_sContext, int i,
                             int enemyBulletSpeed, int enemyBulletUpdateFrequency,
                             int enemyBulletLength, int playerSize, int screenWidth,
                             int screenHeight, int topBoundary, int hpCircleArea,
                             int hpCircleX, int hpCircleY, int hpCircleRadius) {
    // Check collision with player
    int bulletX = app_p->enemyBullets[i].x;
    int bulletY = app_p->enemyBullets[i].y;

    if (bulletX >= app_p->playerX && bulletX <= app_p->playerX + playerSize &&
        bulletY >= app_p->playerY && bulletY <= app_p->playerY + playerSize + enemyBulletLength) {
        // Hit detected - erase bullet
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        if (!app_p->enemyBullets[i].isHorizontal) {
            Graphics_drawLine(g_sContext, bulletX, bulletY, bulletX, bulletY + enemyBulletLength);
        } else {
            Graphics_drawLine(g_sContext, bulletX, bulletY, bulletX + enemyBulletLength, bulletY);
        }

        app_p->enemyBullets[i].active = false;

        // Reduce player HP
        if (app_p->playerHP > 0) {
            app_p->playerHP -= 10;
            if (app_p->playerHP < 0) app_p->playerHP = 0;

            // Update HP display
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
            Graphics_fillCircle(g_sContext, hpCircleX, hpCircleY, hpCircleRadius - 1);
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
            Graphics_drawCircle(g_sContext, hpCircleX, hpCircleY, hpCircleRadius);

            char hpText[4];
            sprintf(hpText, "%d", app_p->playerHP);
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
            Graphics_drawString(g_sContext, (int8_t *)hpText, -1, hpCircleX - 9, hpCircleY - 4, true);
        }
        return;
    }

    // Update movement
    app_p->enemyBullets[i].frameCounter++;

    if (app_p->enemyBullets[i].frameCounter >= enemyBulletUpdateFrequency) {
        app_p->enemyBullets[i].frameCounter = 0;

        // Erase old position
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        if (!app_p->enemyBullets[i].isHorizontal) {
            Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                            app_p->enemyBullets[i].x, app_p->enemyBullets[i].y + enemyBulletLength);
        } else {
            Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                            app_p->enemyBullets[i].x + enemyBulletLength, app_p->enemyBullets[i].y);
        }

        // Move bullet
        app_p->enemyBullets[i].x += enemyBulletSpeed * app_p->enemyBullets[i].directionX;
        app_p->enemyBullets[i].y += enemyBulletSpeed * app_p->enemyBullets[i].directionY;

        // Check if off screen
        if (app_p->enemyBullets[i].x < 0 || app_p->enemyBullets[i].x > screenWidth ||
            app_p->enemyBullets[i].y < topBoundary || app_p->enemyBullets[i].y > screenHeight) {
            app_p->enemyBullets[i].active = false;
            return;
        }
    }

    // Draw bullet
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
    if (!app_p->enemyBullets[i].isHorizontal) {
        Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                        app_p->enemyBullets[i].x, app_p->enemyBullets[i].y + enemyBulletLength);
    } else {
        Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                        app_p->enemyBullets[i].x + enemyBulletLength, app_p->enemyBullets[i].y);
    }
}

// Helper function to update all enemy bullets

void updateAllEnemyBullets(Application* app_p, Graphics_Context* g_sContext,
                          int enemyBulletSpeed, int enemyBulletUpdateFrequency,
                          int enemyBulletLength, int playerSize, int screenWidth,
                          int screenHeight, int topBoundary, int hpCircleArea,
                          int hpCircleX, int hpCircleY, int hpCircleRadius) {
    int i;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (app_p->enemyBullets[i].active) {
            updateSingleEnemyBullet(app_p, g_sContext, i, enemyBulletSpeed,
                                  enemyBulletUpdateFrequency, enemyBulletLength,
                                  playerSize, screenWidth, screenHeight, topBoundary,
                                  hpCircleArea, hpCircleX, hpCircleY, hpCircleRadius);
        }
    }
}

// Helper function to handle player shooting

void handlePlayerShooting(Application* app_p, HAL* hal_p, int playerSize) {
    bool shootButtonIsPressed = Button_isPressed(&hal_p->boosterpackS1);

    if (shootButtonIsPressed && !app_p->shootButtonWasPressed) {
        int i;
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!app_p->bullets[i].active) {
                app_p->bullets[i].x = app_p->playerX + playerSize / 2;
                app_p->bullets[i].y = app_p->playerY;
                app_p->bullets[i].active = true;
                app_p->bullets[i].frameCounter = 0;
                break;
            }
        }
    }

    app_p->shootButtonWasPressed = shootButtonIsPressed;
}

// Helper function to update single player bullet

void updateSinglePlayerBullet(Application* app_p, Graphics_Context* g_sContext, int k,
                              int bulletSpeed, int bulletUpdateFrequency, int bulletLength,
                              int enemySize, int topBoundary, int enemyHpCircleX,
                              int enemyHpCircleY, int enemyHpCircleRadius) {
    // Check collision with enemy
    bool hitEnemy = false;
    if (app_p->bullets[k].x >= app_p->enemyX && app_p->bullets[k].x <= app_p->enemyX + enemySize) {
        int bulletTop = app_p->bullets[k].y;
        int bulletBottom = app_p->bullets[k].y + bulletLength;
        int enemyTop = app_p->enemyY;
        int enemyBottom = app_p->enemyY + enemySize;

        if (bulletBottom >= enemyTop && bulletTop <= enemyBottom) {
            hitEnemy = true;
        }
    }

    if (hitEnemy) {
        // Erase bullet
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        int bulletTop = app_p->bullets[k].y;
        int bulletBottom = app_p->bullets[k].y + bulletLength;
        if (bulletBottom >= 15) {
            int eraseTop = (bulletTop < 15) ? 15 : bulletTop;
            Graphics_drawLine(g_sContext, app_p->bullets[k].x, eraseTop, app_p->bullets[k].x, bulletBottom);
        }

        app_p->bullets[k].active = false;

        // Reduce enemy HP
        if (app_p->enemyHP > 0) {
            app_p->enemyHP -= 10;
            if (app_p->enemyHP < 0) app_p->enemyHP = 0;

            // Update HP display
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
            Graphics_fillCircle(g_sContext, enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius - 1);
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
            Graphics_drawCircle(g_sContext, enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius);

            char enemyHpText[4];
            sprintf(enemyHpText, "%d", app_p->enemyHP);
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
            Graphics_drawString(g_sContext, (int8_t *)enemyHpText, -1, enemyHpCircleX - 9, enemyHpCircleY - 4, true);
        }
        return;
    }

    // Update movement
    app_p->bullets[k].frameCounter++;

    if (app_p->bullets[k].frameCounter >= bulletUpdateFrequency) {
        app_p->bullets[k].frameCounter = 0;

        // Erase old position
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        int bulletTop = app_p->bullets[k].y;
        int bulletBottom = app_p->bullets[k].y + bulletLength;
        if (bulletBottom >= 15) {
            int eraseTop = (bulletTop < 15) ? 15 : bulletTop;
            Graphics_drawLine(g_sContext, app_p->bullets[k].x, eraseTop, app_p->bullets[k].x, bulletBottom);
        }

        // Move bullet up
        app_p->bullets[k].y -= bulletSpeed;

        // Check if reached top
        if (app_p->bullets[k].y <= topBoundary) {
            app_p->bullets[k].active = false;
            return;
        }
    }

    // Draw bullet
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_YELLOW);
    int bulletTop = app_p->bullets[k].y;
    int bulletBottom = app_p->bullets[k].y + bulletLength;
    if (bulletBottom >= 15) {
        int drawTop = (bulletTop < 15) ? 15 : bulletTop;
        Graphics_drawLine(g_sContext, app_p->bullets[k].x, drawTop, app_p->bullets[k].x, bulletBottom);
    }
}


// Helper function to update all player bullets

void updateAllPlayerBullets(Application* app_p, Graphics_Context* g_sContext,
                           int bulletSpeed, int bulletUpdateFrequency, int bulletLength,
                           int enemySize, int topBoundary, int enemyHpCircleX,
                           int enemyHpCircleY, int enemyHpCircleRadius) {
    int k;
    for (k = 0; k < MAX_BULLETS; k++) {
        if (app_p->bullets[k].active) {
            updateSinglePlayerBullet(app_p, g_sContext, k, bulletSpeed,
                                    bulletUpdateFrequency, bulletLength, enemySize,
                                    topBoundary, enemyHpCircleX, enemyHpCircleY,
                                    enemyHpCircleRadius);
        }
    }
}

// Helper function to redraw player and enemy on top

void redrawEntities(Application* app_p, Graphics_Context* g_sContext,
                   int playerSize, int enemySize) {
    // Redraw player
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
    Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){
        app_p->playerX, app_p->playerY,
        app_p->playerX + playerSize, app_p->playerY + playerSize
    });

    // Redraw enemy
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
    Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){
        app_p->enemyX, app_p->enemyY,
        app_p->enemyX + enemySize, app_p->enemyY + enemySize
    });
}

//Main function

void Application_handlePlayScreen(Application* app_p, HAL* hal_p){
    static Graphics_Context g_sContext;

    // Define all constants
    const int playerSize = 8, screenWidth = 128, screenHeight = 128;
    const int moveSpeed = 5, moveUpdateFrequency = 70, topBoundary = 30;
    const int bulletSpeed = 6, bulletLength = 4, bulletUpdateFrequency = 100;
    const int enemySize = 10, enemySpeed = 2, enemyUpdateFrequency = 80;
    const int hpCircleRadius = 11, hpCircleX = 12, hpCircleY = screenHeight - 12, hpCircleArea = 35;
    const int enemyHpCircleRadius = 12, enemyHpCircleX = screenWidth - 14, enemyHpCircleY = 11, enemyHpCircleArea = 35;
    const int enemyBulletSpeed = 5, enemyBulletUpdateFrequency = 80, enemyBulletLength = 4;
    const int patternSpawnInterval = 30, attackCooldown = 200;

    // Initial screen setup - runs once
    if (!app_p->PlayScreenDrawn) {
        drawInitialGameScreen(&g_sContext, app_p, hpCircleX, hpCircleY, hpCircleRadius,
                            enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius,
                            playerSize, enemySize);

        // Initialize bullets
        int i;
        for (i = 0; i < MAX_BULLETS; i++) {
            app_p->bullets[i].active = false;
            app_p->bullets[i].frameCounter = 0;
        }
        app_p->moveFrameCounter = 0;
        app_p->enemyFrameCounter = 0;
        app_p->PlayScreenDrawn = true;
    }

    // Check for game over first - if true, stop everything
    if (handleGameOver(&g_sContext, app_p, hal_p)) {
        return;
    }

    // Update all game systems
    handlePlayerMovement(app_p, &g_sContext, playerSize, screenWidth, screenHeight,
                        topBoundary, hpCircleArea, moveSpeed, moveUpdateFrequency,
                        hpCircleX, hpCircleY, hpCircleRadius);

    updateGameTimer(app_p, &g_sContext);

    updateEnemyPosition(app_p, &g_sContext, enemySize, enemySpeed, enemyUpdateFrequency,
                       screenWidth, enemyHpCircleArea);

    manageEnemyAttackPatterns(app_p, topBoundary, screenWidth, patternSpawnInterval, attackCooldown);

    updateAllEnemyBullets(app_p, &g_sContext, enemyBulletSpeed, enemyBulletUpdateFrequency,
                         enemyBulletLength, playerSize, screenWidth, screenHeight,
                         topBoundary, hpCircleArea, hpCircleX, hpCircleY, hpCircleRadius);

    handlePlayerShooting(app_p, hal_p, playerSize);

    updateAllPlayerBullets(app_p, &g_sContext, bulletSpeed, bulletUpdateFrequency,
                          bulletLength, enemySize, topBoundary, enemyHpCircleX,
                          enemyHpCircleY, enemyHpCircleRadius);

    redrawEntities(app_p, &g_sContext, playerSize, enemySize);
}
