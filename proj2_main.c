// =============================================================================
// main.c — Application Entry Point and Game Logic Implementation
// =============================================================================
// Implements the main game loop, screen handlers, and all gameplay systems
// for the Touhou dodge game running on an MSP432 microcontroller.
//
// Architecture:
//   The application is built around a non-blocking super-loop in main().
//   HAL_refresh() updates all button states, then Application_loop() dispatches
//   to the appropriate screen handler based on the current FSM state.
//   No handler ever blocks — all timing is managed through frame counters
//   and SWTimers checked with if statements rather than while loops.
//
// Screen FSM:
//   TITLE_SCREEN → (3 second timer) → MENU_SCREEN
//   MENU_SCREEN  → (JSB confirm)    → PLAY_SCREEN / INSTRUCTIONS_SCREEN / HIGH_SCORES_SCREEN
//   PLAY_SCREEN  → (game over)      → TITLE_SCREEN
//   INSTRUCTIONS_SCREEN / HIGH_SCORES_SCREEN → (BB2) → MENU_SCREEN
//
// Gameplay Systems (all called every frame from Application_handlePlayScreen):
//   - Player movement via ADC joystick with dead zone and boundary clamping
//   - Player shooting (BB1) with up to MAX_BULLETS simultaneous projectiles
//   - Enemy left/right movement with screen boundary bouncing
//   - Enemy attack pattern manager cycling through 4 patterns:
//       Pattern 0: Vertical rain (bullets fall from top)
//       Pattern 1: Horizontal rain from left
//       Pattern 2: Diagonal from corners
//       Pattern 3: Alternating horizontal from both sides
//   - Bullet collision detection for both player and enemy projectiles
//   - HP tracking with circular displays for player and enemy
//   - Game timer incrementing once per 240 frames (~1 second at loop rate)
//   - High score tracking — top 5 scores stored and displayed
//
// LCD Rendering:
//   The 128x128 Crystalfontz LCD is driven via SPI. To avoid full screen
//   redraws every frame, only changed regions are erased and redrawn.
//   The "drawn" flags in Application prevent redundant LCD initialization.
//
// Originally authored by Aayush Seth, ECE 2564, Virginia Tech, Fall 2025.
// =============================================================================

/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

/* Standard Includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/grlib/grlib.h>
#include <stdlib.h>

#include "HAL/LcdDriver/Crystalfontz128x128_ST7735.h"

/* HAL and Application includes */
#include <Application.h>
#include <HAL/HAL.h>
#include <HAL/Timer.h>

// =============================================================================
// High Score Management
// =============================================================================

// -----------------------------------------------------------------------------
// Application_updateHighScores — Insert a New Score into the Top 5 List
// -----------------------------------------------------------------------------
// Finds the first position where newScore exceeds an existing score, shifts
// lower scores down, and inserts the new score at that position.
// Scores below all existing top 5 scores are silently discarded.
//
// @param app_p     Pointer to the Application containing the high scores array
// @param newScore  The new score to attempt to insert
// -----------------------------------------------------------------------------
void Application_updateHighScores(Application* app_p, int newScore) {
    int insertPos = -1;
    int i;

    // Find the first position where newScore beats an existing score
    for (i = 0; i < 5; i++) {
        if (newScore > app_p->highScores[i]) {
            insertPos = i;
            break;
        }
    }

    // Shift scores down and insert if a valid position was found
    if (insertPos != -1) {
        int j;
        for (j = 4; j > insertPos; j--) {
            app_p->highScores[j] = app_p->highScores[j - 1];
        }
        app_p->highScores[insertPos] = newScore;
    }
}

// -----------------------------------------------------------------------------
// Application_initHighScores — Zero-Initialize the High Scores Array
// -----------------------------------------------------------------------------
// Separated into its own function to keep Application_construct() concise.
//
// @param app_p  Pointer to the Application to initialize
// -----------------------------------------------------------------------------
void Application_initHighScores(Application* app_p) {
    int i;
    for (i = 0; i < 5; i++) {
        app_p->highScores[i] = 0;
    }
}

// =============================================================================
// Non-Blocking LED Test
// =============================================================================
// Required non-blocking check: LED1 mirrors LaunchPad S1 state every loop cycle.
// This verifies the super-loop is non-blocking — if the LED responds instantly
// to the button during intensive game calculations, the loop is non-blocking.

// Configures LED1 (P1.0) as output and LaunchPad S1 (P1.1) as input
static void InitNonBlockingLED() {
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1);
}

// Turns LED1 on when S1 is pressed, off when released — called every loop cycle
static void PollNonBlockingLED() {
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
    if (GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN1) == 0) {
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
    }
}

// =============================================================================
// LCD Graphics Initialization
// =============================================================================

// -----------------------------------------------------------------------------
// initializeGraphics — Initialize the 128x128 LCD and Graphics Context
// -----------------------------------------------------------------------------
// Initializes the Crystalfontz LCD via SPI, sets orientation, configures the
// graphics context with white foreground, black background, and fixed 6x8 font,
// then clears the display. Call this once before drawing to a new screen.
//
// @param g_sContext_p  Pointer to the Graphics_Context to initialize
// -----------------------------------------------------------------------------
void initializeGraphics(Graphics_Context *g_sContext_p) {
    Crystalfontz128x128_Init();
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);
    Graphics_initContext(g_sContext_p, &g_sCrystalfontz128x128, &g_sCrystalfontz128x128_funcs);
    Graphics_setForegroundColor(g_sContext_p, GRAPHICS_COLOR_WHITE);
    Graphics_setBackgroundColor(g_sContext_p, GRAPHICS_COLOR_BLACK);
    Graphics_setFont(g_sContext_p, &g_sFontFixed6x8);
    Graphics_clearDisplay(g_sContext_p);
}

// =============================================================================
// main — System Entry Point
// =============================================================================
// Initializes all hardware, constructs the HAL and Application objects,
// then runs the non-blocking super-loop indefinitely.
// =============================================================================
int main(void) {
    // Stop Watchdog Timer — MUST be the first line of main()
    WDT_A_holdTimer();

    // Initialize system clock and hardware timer (required before any SWTimers)
    InitSystemTiming();

    // Construct the main HAL and Application objects
    HAL hal = HAL_construct();
    Application app = Application_construct();

    // Initialize non-blocking LED test
    InitNonBlockingLED();

    // -------------------------------------------------------------------------
    // Main Super-Loop
    // Non-blocking polling architecture — no while loops inside handlers
    // -------------------------------------------------------------------------
    while (true) {
        PollNonBlockingLED();       // Non-blocking LED check (required)
        HAL_refresh(&hal);          // Update all button debounce FSMs
        Application_loop(&app, &hal); // Dispatch to current screen handler
    }
}

// =============================================================================
// Application_construct — Initialize All Application State
// =============================================================================
// Sets up screen FSM, player/enemy state, bullet arrays, attack pattern
// variables, and the game timer. Called once before the super-loop starts.
// =============================================================================
Application Application_construct() {
    Application app;

    Application_initHighScores(&app);

    // -------------------------------------------------------------------------
    // Screen State Initialization
    // -------------------------------------------------------------------------
    app.currentScreen           = TITLE_SCREEN;
    app.titleScreenDrawn        = false;
    app.titleTimer              = SWTimer_construct(3000); // 3-second title display
    SWTimer_start(&app.titleTimer);
    app.menuScreenDrawn         = false;
    app.menuSelection           = 0;                       // Default: first menu option
    app.instructionsScreenDrawn = false;
    app.HighScoresScreenDrawn   = false;
    app.PlayScreenDrawn         = false;
    app.gameOverScreenDrawn     = false;

    // -------------------------------------------------------------------------
    // Player State Initialization
    // -------------------------------------------------------------------------
    app.playerX              = 56;      // Center horizontally on 128px screen
    app.playerY              = 56;      // Center vertically
    app.playerHP             = 25;      // Starting health
    app.moveFrameCounter     = 0;
    app.shootButtonWasPressed = false;

    // Initialize all player bullets as inactive
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        app.bullets[i].active       = false;
        app.bullets[i].x            = 0;
        app.bullets[i].y            = 0;
        app.bullets[i].frameCounter = 0;
    }

    // -------------------------------------------------------------------------
    // Enemy State Initialization
    // -------------------------------------------------------------------------
    app.enemyHP            = 25;    // Starting health
    app.enemyX             = 59;    // Near center horizontally
    app.enemyY             = 25;    // Just below the game title bar
    app.enemyDirection     = 1;     // Start moving right
    app.enemyFrameCounter  = 0;

    // Initialize all enemy bullets as inactive
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        app.enemyBullets[i].active       = false;
        app.enemyBullets[i].x            = 0;
        app.enemyBullets[i].y            = 0;
        app.enemyBullets[i].frameCounter = 0;
        app.enemyBullets[i].directionX   = 0;
        app.enemyBullets[i].directionY   = 1;  // Default: downward
        app.enemyBullets[i].isHorizontal = false;
    }

    // -------------------------------------------------------------------------
    // Attack Pattern Initialization
    // -------------------------------------------------------------------------
    app.currentAttackPattern  = 0;      // Start with pattern 0 (vertical rain)
    app.patternFrameCounter   = 0;
    app.attackCooldownCounter = 0;
    app.patternActive         = false;  // No pattern active at start

    // -------------------------------------------------------------------------
    // Game Timer Initialization
    // -------------------------------------------------------------------------
    app.gameTimer        = 0;
    app.timerFrameCounter = 0;

    // Seed random number generator for randomized bullet spawning
    srand(1);

    return app;
}

// =============================================================================
// Application_loop — Main FSM Dispatcher
// =============================================================================
// Called every super-loop cycle. Dispatches to the appropriate screen handler
// based on app_p->currentScreen. Each handler is responsible for both rendering
// and input handling for its screen state.
// =============================================================================
void Application_loop(Application* app_p, HAL* hal_p) {
    switch (app_p->currentScreen) {
        case TITLE_SCREEN:
            Application_handleTitleScreen(app_p, hal_p);
            break;
        case MENU_SCREEN:
            Application_handleMenuScreen(app_p, hal_p);
            break;
        case INSTRUCTIONS_SCREEN:
            Application_handleInstructionsScreen(app_p, hal_p);
            break;
        case HIGH_SCORES_SCREEN:
            Application_handleHighScoresScreen(app_p, hal_p);
            break;
        case PLAY_SCREEN:
            Application_handlePlayScreen(app_p, hal_p);
            break;
    }
}

// =============================================================================
// Screen Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// Application_handleTitleScreen — Display Title for 3 Seconds Then Transition
// -----------------------------------------------------------------------------
// Draws the title screen once (guarded by titleScreenDrawn flag to prevent
// redundant LCD writes), then polls the SWTimer. When 3 seconds elapse,
// transitions to MENU_SCREEN.
// -----------------------------------------------------------------------------
void Application_handleTitleScreen(Application* app_p, HAL* hal_p) {
    if (!app_p->titleScreenDrawn) {
        Graphics_Context g_sContext;
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);

        Graphics_drawString(&g_sContext, (int8_t *)"ECE2564",           -1, 10, 10, true);
        Graphics_drawString(&g_sContext, (int8_t *)"Project 2",         -1, 10, 25, true);
        Graphics_drawString(&g_sContext, (int8_t *)"----------------",  -1, 10, 35, true);
        Graphics_drawString(&g_sContext, (int8_t *)"Touhou",            -1, 10, 60, true);

        app_p->titleScreenDrawn = true;
    }

    // Transition to menu after 3 seconds
    if (SWTimer_expired(&app_p->titleTimer)) {
        app_p->currentScreen    = MENU_SCREEN;
        app_p->titleScreenDrawn = false;
    }
}

// -----------------------------------------------------------------------------
// Application_handleMenuScreen — Navigate and Select Menu Options
// -----------------------------------------------------------------------------
// Draws the menu once, then redraws only the option list when the selection
// changes (tracked by lastSelection static variable). BB1 moves cursor up,
// BB2 moves cursor down (wrapping), JSB confirms selection and transitions
// to the corresponding screen.
// -----------------------------------------------------------------------------
void Application_handleMenuScreen(Application* app_p, HAL* hal_p) {
    static Graphics_Context g_sContext;

    // Draw menu header once
    if (!app_p->menuScreenDrawn) {
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_CYAN);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_drawString(&g_sContext, (int8_t *)"MENU", -1, 5, 5, true);
        app_p->menuScreenDrawn = true;
    }

    static int lastSelection = -1;

    // Redraw option list only when selection changes — avoids full screen refresh
    if (lastSelection != app_p->menuSelection) {
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_CYAN);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);

        if (app_p->menuSelection == 0) {
            Graphics_drawString(&g_sContext, (int8_t *)"> Play",         -1, 5, 40, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  Instructions", -1, 5, 55, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  High Scores",  -1, 5, 70, true);
        } else if (app_p->menuSelection == 1) {
            Graphics_drawString(&g_sContext, (int8_t *)"  Play",         -1, 5, 40, true);
            Graphics_drawString(&g_sContext, (int8_t *)"> Instructions", -1, 5, 55, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  High Scores",  -1, 5, 70, true);
        } else {
            Graphics_drawString(&g_sContext, (int8_t *)"  Play",         -1, 5, 40, true);
            Graphics_drawString(&g_sContext, (int8_t *)"  Instructions", -1, 5, 55, true);
            Graphics_drawString(&g_sContext, (int8_t *)"> High Scores",  -1, 5, 70, true);
        }

        lastSelection = app_p->menuSelection;
    }

    // BB1: move cursor up (wraps from top to bottom)
    if (Button_isTapped(&hal_p->boosterpackS1)) {
        app_p->menuSelection--;
        if (app_p->menuSelection < 0) app_p->menuSelection = 2;
    }

    // BB2: move cursor down (wraps from bottom to top)
    if (Button_isTapped(&hal_p->boosterpackS2)) {
        app_p->menuSelection++;
        if (app_p->menuSelection > 2) app_p->menuSelection = 0;
    }

    // JSB: confirm selection and transition to chosen screen
    if (Button_isTapped(&hal_p->boosterpackJS)) {
        app_p->menuScreenDrawn = false;
        lastSelection = -1;

        switch (app_p->menuSelection) {
            case 0: app_p->currentScreen = PLAY_SCREEN;         break;
            case 1: app_p->currentScreen = INSTRUCTIONS_SCREEN; break;
            case 2: app_p->currentScreen = HIGH_SCORES_SCREEN;  break;
        }
    }
}

// -----------------------------------------------------------------------------
// Application_handleInstructionsScreen — Display Controls and Return to Menu
// -----------------------------------------------------------------------------
// Draws the instructions screen once. BB2 returns to MENU_SCREEN.
// -----------------------------------------------------------------------------
void Application_handleInstructionsScreen(Application* app_p, HAL* hal_p) {
    if (!app_p->instructionsScreenDrawn) {
        Graphics_Context g_sContext;
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);

        Graphics_drawString(&g_sContext, (int8_t *)"Instructions",       -1, 20, 5,   true);
        Graphics_drawLine(&g_sContext, 5, 15, 123, 15);
        Graphics_drawString(&g_sContext, (int8_t *)"Move the character", -1, 5,  20,  true);
        Graphics_drawString(&g_sContext, (int8_t *)"to avoid being hit", -1, 5,  30,  true);
        Graphics_drawString(&g_sContext, (int8_t *)"by the bullets.",    -1, 5,  40,  true);
        Graphics_drawString(&g_sContext, (int8_t *)"Press BB1 to fire",  -1, 5,  50,  true);
        Graphics_drawString(&g_sContext, (int8_t *)"at the enemy.",      -1, 5,  60,  true);
        Graphics_drawString(&g_sContext, (int8_t *)"First one to run",   -1, 5,  70,  true);
        Graphics_drawString(&g_sContext, (int8_t *)"out of health loses.",-1, 5, 80,  true);
        Graphics_drawString(&g_sContext, (int8_t *)"Press BB2 to return",-1, 5,  110, true);

        app_p->instructionsScreenDrawn = true;
    }

    // BB2: return to menu
    if (Button_isTapped(&hal_p->boosterpackS2)) {
        app_p->currentScreen            = MENU_SCREEN;
        app_p->instructionsScreenDrawn  = false;
        app_p->menuScreenDrawn          = false;
    }
}

// -----------------------------------------------------------------------------
// Application_handleHighScoresScreen — Display Top 5 Scores and Return to Menu
// -----------------------------------------------------------------------------
// Draws the high scores screen once, rendering all 5 scores from the array.
// BB2 returns to MENU_SCREEN.
// -----------------------------------------------------------------------------
void Application_handleHighScoresScreen(Application* app_p, HAL* hal_p) {
    if (!app_p->HighScoresScreenDrawn) {
        Graphics_Context g_sContext;
        initializeGraphics(&g_sContext);
        Graphics_clearDisplay(&g_sContext);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);

        Graphics_drawString(&g_sContext, (int8_t *)"High Scores", -1, 25, 5, true);
        Graphics_drawLine(&g_sContext, 5, 15, 123, 15);

        // Render all 5 high scores from the array
        char scoreText[20];
        int i;
        for (i = 0; i < 5; i++) {
            sprintf(scoreText, "%d. %d", i + 1, app_p->highScores[i]);
            Graphics_drawString(&g_sContext, (int8_t *)scoreText, -1, 40, 25 + (i * 15), true);
        }

        Graphics_drawString(&g_sContext, (int8_t *)"Press BB2 to return", -1, 5, 110, true);
        app_p->HighScoresScreenDrawn = true;
    }

    // BB2: return to menu
    if (Button_isTapped(&hal_p->boosterpackS2)) {
        app_p->currentScreen         = MENU_SCREEN;
        app_p->HighScoresScreenDrawn = false;
        app_p->menuScreenDrawn       = false;
    }
}

// =============================================================================
// Gameplay Helper Functions
// =============================================================================

// -----------------------------------------------------------------------------
// drawInitialGameScreen — Render the Full Game Screen on First Entry
// -----------------------------------------------------------------------------
// Called once when entering PLAY_SCREEN. Draws the background, timer, HP
// circles for player and enemy, and initial player/enemy rectangles.
// Subsequent frames only update changed regions to avoid full redraws.
// -----------------------------------------------------------------------------
void drawInitialGameScreen(Graphics_Context* g_sContext, Application* app_p,
                           int hpCircleX, int hpCircleY, int hpCircleRadius,
                           int enemyHpCircleX, int enemyHpCircleY, int enemyHpCircleRadius,
                           int playerSize, int enemySize) {
    initializeGraphics(g_sContext);
    Graphics_clearDisplay(g_sContext);

    // Draw game title and timer
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
    char timerText[10];
    sprintf(timerText, "T:%d", app_p->gameTimer);
    Graphics_drawString(g_sContext, (int8_t *)timerText, -1, 50, 5, true);
    Graphics_drawString(g_sContext, (int8_t *)"Game", -1, 5, 5, true);

    // Draw player HP circle (bottom-left, cyan)
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
    Graphics_drawCircle(g_sContext, hpCircleX, hpCircleY, hpCircleRadius);
    char hpText[4];
    sprintf(hpText, "%d", app_p->playerHP);
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_drawString(g_sContext, (int8_t *)hpText, -1, hpCircleX - 9, hpCircleY - 4, true);

    // Draw enemy HP circle (top-right, red)
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
    Graphics_drawCircle(g_sContext, enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius);
    char enemyHpText[4];
    sprintf(enemyHpText, "%d", app_p->enemyHP);
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_drawString(g_sContext, (int8_t *)enemyHpText, -1, enemyHpCircleX - 9, enemyHpCircleY - 4, true);

    // Draw player (cyan rectangle) and enemy (red rectangle)
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

// -----------------------------------------------------------------------------
// handleGameOver — Check Win/Lose Condition and Display Result Screen
// -----------------------------------------------------------------------------
// Returns true if either player or enemy HP has reached zero, halting all
// further game updates. Displays WIN/LOSE, final score, and waits for JSB
// to restart the game from the title screen.
// Score = elapsed game time + remaining player HP
// -----------------------------------------------------------------------------
bool handleGameOver(Graphics_Context* g_sContext, Application* app_p, HAL* hal_p) {
    if (app_p->playerHP <= 0 || app_p->enemyHP <= 0) {
        if (!app_p->gameOverScreenDrawn) {
            int finalScore = app_p->gameTimer + app_p->playerHP;
            Application_updateHighScores(app_p, finalScore);

            // Clear the bottom area for game over message
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
            Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){0, 92, 128, 128});

            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
            Graphics_setBackgroundColor(g_sContext, GRAPHICS_COLOR_BLACK);

            // WIN if enemy died, LOSE if player died
            if (app_p->enemyHP <= 0) {
                Graphics_drawString(g_sContext, (int8_t *)"WIN!",     -1, 48, 95,  true);
            } else {
                Graphics_drawString(g_sContext, (int8_t *)"LOSE",     -1, 48, 95,  true);
            }

            char scoreText[15];
            sprintf(scoreText, "S:%d", finalScore);
            Graphics_drawString(g_sContext, (int8_t *)scoreText,      -1, 45, 106, true);
            Graphics_drawString(g_sContext, (int8_t *)"Press JSB",    -1, 35, 117, true);

            app_p->gameOverScreenDrawn = true;
        }

        // JSB: reset all game state and return to title screen
        if (Button_isPressed(&hal_p->boosterpackJS)) {
            app_p->currentScreen    = TITLE_SCREEN;
            app_p->titleScreenDrawn = false;
            app_p->playerHP         = 100;
            app_p->enemyHP          = 100;
            app_p->gameTimer        = 0;
            app_p->PlayScreenDrawn  = false;
            app_p->gameOverScreenDrawn = false;
        }

        return true;  // Game is over — caller should skip remaining updates
    }
    return false;
}

// -----------------------------------------------------------------------------
// handlePlayerMovement — Read Joystick ADC and Move Player with Boundary Clamping
// -----------------------------------------------------------------------------
// Reads raw 14-bit ADC values from joystick X/Y axes, applies a dead zone
// around center (8192), and updates player position every moveUpdateFrequency
// frames. Clamps to screen bounds and prevents overlap with the HP circle.
// Only redraws when position actually changes to minimize LCD writes.
// -----------------------------------------------------------------------------
void handlePlayerMovement(Application* app_p, Graphics_Context* g_sContext,
                          int playerSize, int screenWidth, int screenHeight,
                          int topBoundary, int hpCircleArea, int moveSpeed,
                          int moveUpdateFrequency, int hpCircleX, int hpCircleY, int hpCircleRadius) {
    unsigned vx = ADC14_getResult(ADC_MEM0); // Joystick X axis
    unsigned vy = ADC14_getResult(ADC_MEM1); // Joystick Y axis

    const int CENTER    = 8192;  // Midpoint of 14-bit ADC range
    const int DEAD_ZONE = 1000;  // Minimum deflection to register movement

    int oldX = app_p->playerX;
    int oldY = app_p->playerY;

    app_p->moveFrameCounter++;

    // Update position only every moveUpdateFrequency frames to control speed
    if (app_p->moveFrameCounter >= moveUpdateFrequency) {
        app_p->moveFrameCounter = 0;

        if (vx > CENTER + DEAD_ZONE) app_p->playerX += moveSpeed;
        if (vx < CENTER - DEAD_ZONE) app_p->playerX -= moveSpeed;
        if (vy < CENTER - DEAD_ZONE) app_p->playerY += moveSpeed; // Y axis inverted
        if (vy > CENTER + DEAD_ZONE) app_p->playerY -= moveSpeed;
    }

    // Clamp player to screen boundaries
    if (app_p->playerX < 0)                         app_p->playerX = 0;
    if (app_p->playerX > screenWidth - playerSize)  app_p->playerX = screenWidth - playerSize;
    if (app_p->playerY < topBoundary)               app_p->playerY = topBoundary;
    if (app_p->playerY > screenHeight - playerSize) app_p->playerY = screenHeight - playerSize;

    // Prevent player from overlapping the HP circle in the bottom-left corner
    if (app_p->playerX < hpCircleArea && app_p->playerY > screenHeight - hpCircleArea) {
        if (oldX >= hpCircleArea) {
            app_p->playerX = hpCircleArea;
        } else if (oldY <= screenHeight - hpCircleArea) {
            app_p->playerY = screenHeight - hpCircleArea;
        } else {
            // Push in the direction of least resistance
            int distRight = hpCircleArea - app_p->playerX;
            int distUp    = (screenHeight - hpCircleArea) - app_p->playerY;
            if (distRight < distUp) app_p->playerX = hpCircleArea;
            else                    app_p->playerY = screenHeight - hpCircleArea;
        }
    }

    // Redraw only if position changed
    if (oldX != app_p->playerX || oldY != app_p->playerY) {
        // Erase old position
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){oldX, oldY, oldX + playerSize, oldY + playerSize});

        // Draw new position
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_CYAN);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){
            app_p->playerX, app_p->playerY,
            app_p->playerX + playerSize, app_p->playerY + playerSize
        });

        // Redraw HP circle if player passed through its area
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

// -----------------------------------------------------------------------------
// updateGameTimer — Increment Game Timer Once Per ~240 Frames
// -----------------------------------------------------------------------------
// Counts super-loop cycles and increments the displayed timer approximately
// once per second. Erases and redraws only the timer text region.
// -----------------------------------------------------------------------------
void updateGameTimer(Application* app_p, Graphics_Context* g_sContext) {
    app_p->timerFrameCounter++;
    if (app_p->timerFrameCounter >= 240) {
        app_p->timerFrameCounter = 0;
        app_p->gameTimer++;

        // Erase old timer text and redraw with new value
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_fillRectangle(g_sContext, &(Graphics_Rectangle){50, 5, 90, 13});

        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
        char timerText[10];
        sprintf(timerText, "T:%d", app_p->gameTimer);
        Graphics_drawString(g_sContext, (int8_t *)timerText, -1, 50, 5, true);
    }
}

// -----------------------------------------------------------------------------
// updateEnemyPosition — Move Enemy Left/Right and Bounce off Screen Edges
// -----------------------------------------------------------------------------
// Updates enemy X position every enemyUpdateFrequency frames, reversing
// direction when the enemy reaches either screen boundary. Erases and redraws
// only when the enemy actually moves.
// -----------------------------------------------------------------------------
void updateEnemyPosition(Application* app_p, Graphics_Context* g_sContext,
                         int enemySize, int enemySpeed, int enemyUpdateFrequency,
                         int screenWidth, int enemyHpCircleArea) {
    int oldEnemyX = app_p->enemyX;
    app_p->enemyFrameCounter++;

    if (app_p->enemyFrameCounter >= enemyUpdateFrequency) {
        app_p->enemyFrameCounter = 0;
        app_p->enemyX += enemySpeed * app_p->enemyDirection;

        // Bounce off left edge
        if (app_p->enemyX <= 0) {
            app_p->enemyX = 0;
            app_p->enemyDirection = 1;
        }
        // Bounce off right edge (avoid HP circle area)
        if (app_p->enemyX >= screenWidth - enemyHpCircleArea) {
            app_p->enemyX = screenWidth - enemyHpCircleArea;
            app_p->enemyDirection = -1;
        }
    }

    // Redraw only if position changed
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

// =============================================================================
// Enemy Attack Pattern Spawners
// Each pattern spawns bullets at timed intervals using patternFrameCounter.
// Sets patternActive = false when all bullets for the pattern have been spawned.
// =============================================================================

// Pattern 0 — Vertical Rain: 8 bullets falling from evenly-spaced X positions
void spawnPattern1(Application* app_p, int topBoundary) {
    if (app_p->patternFrameCounter % 40 == 0) {
        int bulletIndex = app_p->patternFrameCounter / 40;
        if (bulletIndex < 8) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    app_p->enemyBullets[i].x            = 10 + (bulletIndex * 15);
                    app_p->enemyBullets[i].y            = topBoundary;
                    app_p->enemyBullets[i].directionX   = 0;
                    app_p->enemyBullets[i].directionY   = 1;  // Downward
                    app_p->enemyBullets[i].isHorizontal = false;
                    app_p->enemyBullets[i].active       = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// Pattern 1 — Horizontal Rain from Left: 8 bullets crossing from left edge
void spawnPattern2(Application* app_p, int topBoundary) {
    if (app_p->patternFrameCounter % 25 == 0) {
        int bulletIndex = app_p->patternFrameCounter / 25;
        if (bulletIndex < 8) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    app_p->enemyBullets[i].x            = 0;
                    app_p->enemyBullets[i].y            = topBoundary + (bulletIndex * 12);
                    app_p->enemyBullets[i].directionX   = 1;  // Rightward
                    app_p->enemyBullets[i].directionY   = 0;
                    app_p->enemyBullets[i].isHorizontal = true;
                    app_p->enemyBullets[i].active       = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// Pattern 2 — Diagonal from Corners: alternating bullets from top-left and top-right
void spawnPattern3(Application* app_p, int topBoundary, int screenWidth, int patternSpawnInterval) {
    if (app_p->patternFrameCounter % patternSpawnInterval == 0) {
        int bulletIndex = app_p->patternFrameCounter / patternSpawnInterval;
        if (bulletIndex < 16) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    if (bulletIndex % 2 == 0) {
                        // Even: spawn from top-left, move diagonally right+down
                        app_p->enemyBullets[i].x          = 0;
                        app_p->enemyBullets[i].y          = topBoundary;
                        app_p->enemyBullets[i].directionX = 1;
                        app_p->enemyBullets[i].directionY = 1;
                    } else {
                        // Odd: spawn from top-right, move diagonally left+down
                        app_p->enemyBullets[i].x          = screenWidth - 1;
                        app_p->enemyBullets[i].y          = topBoundary;
                        app_p->enemyBullets[i].directionX = -1;
                        app_p->enemyBullets[i].directionY = 1;
                    }
                    app_p->enemyBullets[i].isHorizontal = false;
                    app_p->enemyBullets[i].active       = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// Pattern 3 — Alternating Horizontal from Both Sides: bullets crossing from left and right
void spawnPattern4(Application* app_p, int topBoundary, int screenWidth, int patternSpawnInterval) {
    if (app_p->patternFrameCounter % patternSpawnInterval == 0) {
        int bulletIndex = app_p->patternFrameCounter / patternSpawnInterval;
        if (bulletIndex < 12) {
            int i;
            for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!app_p->enemyBullets[i].active) {
                    if (bulletIndex % 2 == 0) {
                        // Even: spawn from left edge, move right
                        app_p->enemyBullets[i].x          = 0;
                        app_p->enemyBullets[i].y          = topBoundary + (bulletIndex * 8);
                        app_p->enemyBullets[i].directionX = 1;
                        app_p->enemyBullets[i].directionY = 0;
                    } else {
                        // Odd: spawn from right edge, move left
                        app_p->enemyBullets[i].x          = screenWidth - 1;
                        app_p->enemyBullets[i].y          = topBoundary + (bulletIndex * 8);
                        app_p->enemyBullets[i].directionX = -1;
                        app_p->enemyBullets[i].directionY = 0;
                    }
                    app_p->enemyBullets[i].isHorizontal = true;
                    app_p->enemyBullets[i].active       = true;
                    app_p->enemyBullets[i].frameCounter = 0;
                    break;
                }
            }
        } else {
            app_p->patternActive = false;
        }
    }
}

// -----------------------------------------------------------------------------
// manageEnemyAttackPatterns — Cycle Through Attack Patterns with Cooldown
// -----------------------------------------------------------------------------
// Waits for all active enemy bullets to clear, then counts down an attack
// cooldown. When ready, advances to the next pattern (0→1→2→3→0) and
// starts spawning. Dispatches to the appropriate spawn function each frame
// while a pattern is active.
// -----------------------------------------------------------------------------
void manageEnemyAttackPatterns(Application* app_p, int topBoundary, int screenWidth,
                                int patternSpawnInterval, int attackCooldown) {
    // Check if all enemy bullets have left the screen
    bool allEnemyBulletsInactive = true;
    int j;
    for (j = 0; j < MAX_ENEMY_BULLETS; j++) {
        if (app_p->enemyBullets[j].active) {
            allEnemyBulletsInactive = false;
            break;
        }
    }

    // Count down cooldown between patterns once all bullets are cleared
    if (!app_p->patternActive && allEnemyBulletsInactive) {
        app_p->attackCooldownCounter++;
        if (app_p->attackCooldownCounter >= attackCooldown) {
            app_p->patternActive          = true;
            app_p->patternFrameCounter    = 0;
            app_p->attackCooldownCounter  = 0;
            app_p->currentAttackPattern   = (app_p->currentAttackPattern + 1) % 4;
        }
    }

    // Dispatch to active pattern's spawn function
    if (app_p->patternActive) {
        app_p->patternFrameCounter++;

        switch (app_p->currentAttackPattern) {
            case 0: spawnPattern1(app_p, topBoundary);                              break;
            case 1: spawnPattern2(app_p, topBoundary);                              break;
            case 2: spawnPattern3(app_p, topBoundary, screenWidth, patternSpawnInterval); break;
            case 3: spawnPattern4(app_p, topBoundary, screenWidth, patternSpawnInterval); break;
        }
    }
}

// -----------------------------------------------------------------------------
// updateSingleEnemyBullet — Move, Draw, and Check Collision for One Enemy Bullet
// -----------------------------------------------------------------------------
// Checks collision with the player first — if hit, erases bullet and reduces
// player HP by 10, updating the HP circle display. Otherwise advances the
// bullet's position every enemyBulletUpdateFrequency frames, erasing the old
// position and drawing the new one. Deactivates when off-screen.
// Bullet shape: vertical line if !isHorizontal, horizontal line if isHorizontal.
// -----------------------------------------------------------------------------
void updateSingleEnemyBullet(Application* app_p, Graphics_Context* g_sContext, int i,
                              int enemyBulletSpeed, int enemyBulletUpdateFrequency,
                              int enemyBulletLength, int playerSize, int screenWidth,
                              int screenHeight, int topBoundary, int hpCircleArea,
                              int hpCircleX, int hpCircleY, int hpCircleRadius) {
    int bulletX = app_p->enemyBullets[i].x;
    int bulletY = app_p->enemyBullets[i].y;

    // Collision check: bullet overlaps player bounding box
    if (bulletX >= app_p->playerX && bulletX <= app_p->playerX + playerSize &&
        bulletY >= app_p->playerY && bulletY <= app_p->playerY + playerSize + enemyBulletLength) {

        // Erase bullet on hit
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        if (!app_p->enemyBullets[i].isHorizontal)
            Graphics_drawLine(g_sContext, bulletX, bulletY, bulletX, bulletY + enemyBulletLength);
        else
            Graphics_drawLine(g_sContext, bulletX, bulletY, bulletX + enemyBulletLength, bulletY);

        app_p->enemyBullets[i].active = false;

        // Reduce player HP and update display
        if (app_p->playerHP > 0) {
            app_p->playerHP -= 10;
            if (app_p->playerHP < 0) app_p->playerHP = 0;

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

    // Advance bullet position every enemyBulletUpdateFrequency frames
    app_p->enemyBullets[i].frameCounter++;

    if (app_p->enemyBullets[i].frameCounter >= enemyBulletUpdateFrequency) {
        app_p->enemyBullets[i].frameCounter = 0;

        // Erase at old position
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        if (!app_p->enemyBullets[i].isHorizontal)
            Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                              app_p->enemyBullets[i].x, app_p->enemyBullets[i].y + enemyBulletLength);
        else
            Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                              app_p->enemyBullets[i].x + enemyBulletLength, app_p->enemyBullets[i].y);

        // Move bullet by speed in its direction
        app_p->enemyBullets[i].x += enemyBulletSpeed * app_p->enemyBullets[i].directionX;
        app_p->enemyBullets[i].y += enemyBulletSpeed * app_p->enemyBullets[i].directionY;

        // Deactivate if off-screen
        if (app_p->enemyBullets[i].x < 0 || app_p->enemyBullets[i].x > screenWidth ||
            app_p->enemyBullets[i].y < topBoundary || app_p->enemyBullets[i].y > screenHeight) {
            app_p->enemyBullets[i].active = false;
            return;
        }
    }

    // Draw bullet at current position
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
    if (!app_p->enemyBullets[i].isHorizontal)
        Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                          app_p->enemyBullets[i].x, app_p->enemyBullets[i].y + enemyBulletLength);
    else
        Graphics_drawLine(g_sContext, app_p->enemyBullets[i].x, app_p->enemyBullets[i].y,
                          app_p->enemyBullets[i].x + enemyBulletLength, app_p->enemyBullets[i].y);
}

// -----------------------------------------------------------------------------
// updateAllEnemyBullets — Iterate and Update All Active Enemy Bullets
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// handlePlayerShooting — Spawn a Player Bullet on BB1 Rising Edge
// -----------------------------------------------------------------------------
// Detects BB1 press using edge detection (was not pressed, now pressed) to
// prevent continuous bullet spam. Spawns one bullet at the top-center of the
// player sprite into the first available inactive bullet slot.
// -----------------------------------------------------------------------------
void handlePlayerShooting(Application* app_p, HAL* hal_p, int playerSize) {
    bool shootButtonIsPressed = Button_isPressed(&hal_p->boosterpackS1);

    // Rising edge detection — only fire on the first frame the button is pressed
    if (shootButtonIsPressed && !app_p->shootButtonWasPressed) {
        int i;
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!app_p->bullets[i].active) {
                app_p->bullets[i].x            = app_p->playerX + playerSize / 2;
                app_p->bullets[i].y            = app_p->playerY;
                app_p->bullets[i].active       = true;
                app_p->bullets[i].frameCounter = 0;
                break;  // Only spawn one bullet per press
            }
        }
    }

    app_p->shootButtonWasPressed = shootButtonIsPressed;
}

// -----------------------------------------------------------------------------
// updateSinglePlayerBullet — Move, Draw, and Check Collision for One Player Bullet
// -----------------------------------------------------------------------------
// Checks collision with the enemy first — if hit, reduces enemy HP by 10 and
// updates the enemy HP circle display. Otherwise advances bullet upward every
// bulletUpdateFrequency frames, clipping draw calls to stay below the top
// boundary (y >= 15). Deactivates when the bullet reaches the top boundary.
// -----------------------------------------------------------------------------
void updateSinglePlayerBullet(Application* app_p, Graphics_Context* g_sContext, int k,
                               int bulletSpeed, int bulletUpdateFrequency, int bulletLength,
                               int enemySize, int topBoundary, int enemyHpCircleX,
                               int enemyHpCircleY, int enemyHpCircleRadius) {
    // Collision check: bullet X within enemy width, bullet Y overlaps enemy height
    bool hitEnemy = false;
    if (app_p->bullets[k].x >= app_p->enemyX && app_p->bullets[k].x <= app_p->enemyX + enemySize) {
        int bulletTop    = app_p->bullets[k].y;
        int bulletBottom = app_p->bullets[k].y + bulletLength;
        int enemyTop     = app_p->enemyY;
        int enemyBottom  = app_p->enemyY + enemySize;

        if (bulletBottom >= enemyTop && bulletTop <= enemyBottom) {
            hitEnemy = true;
        }
    }

    if (hitEnemy) {
        // Erase bullet (clipped to stay within game area)
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        int bulletTop    = app_p->bullets[k].y;
        int bulletBottom = app_p->bullets[k].y + bulletLength;
        if (bulletBottom >= 15) {
            int eraseTop = (bulletTop < 15) ? 15 : bulletTop;
            Graphics_drawLine(g_sContext, app_p->bullets[k].x, eraseTop,
                              app_p->bullets[k].x, bulletBottom);
        }

        app_p->bullets[k].active = false;

        // Reduce enemy HP and update display
        if (app_p->enemyHP > 0) {
            app_p->enemyHP -= 10;
            if (app_p->enemyHP < 0) app_p->enemyHP = 0;

            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
            Graphics_fillCircle(g_sContext, enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius - 1);
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_RED);
            Graphics_drawCircle(g_sContext, enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius);

            char enemyHpText[4];
            sprintf(enemyHpText, "%d", app_p->enemyHP);
            Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_WHITE);
            Graphics_drawString(g_sContext, (int8_t *)enemyHpText, -1,
                                enemyHpCircleX - 9, enemyHpCircleY - 4, true);
        }
        return;
    }

    // Advance bullet upward every bulletUpdateFrequency frames
    app_p->bullets[k].frameCounter++;

    if (app_p->bullets[k].frameCounter >= bulletUpdateFrequency) {
        app_p->bullets[k].frameCounter = 0;

        // Erase at old position (clipped)
        Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_BLACK);
        int bulletTop    = app_p->bullets[k].y;
        int bulletBottom = app_p->bullets[k].y + bulletLength;
        if (bulletBottom >= 15) {
            int eraseTop = (bulletTop < 15) ? 15 : bulletTop;
            Graphics_drawLine(g_sContext, app_p->bullets[k].x, eraseTop,
                              app_p->bullets[k].x, bulletBottom);
        }

        // Move bullet up
        app_p->bullets[k].y -= bulletSpeed;

        // Deactivate if reached top boundary
        if (app_p->bullets[k].y <= topBoundary) {
            app_p->bullets[k].active = false;
            return;
        }
    }

    // Draw bullet at current position (clipped to game area)
    Graphics_setForegroundColor(g_sContext, GRAPHICS_COLOR_YELLOW);
    int bulletTop    = app_p->bullets[k].y;
    int bulletBottom = app_p->bullets[k].y + bulletLength;
    if (bulletBottom >= 15) {
        int drawTop = (bulletTop < 15) ? 15 : bulletTop;
        Graphics_drawLine(g_sContext, app_p->bullets[k].x, drawTop,
                          app_p->bullets[k].x, bulletBottom);
    }
}

// -----------------------------------------------------------------------------
// updateAllPlayerBullets — Iterate and Update All Active Player Bullets
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// redrawEntities — Redraw Player and Enemy on Top of All Other Graphics
// -----------------------------------------------------------------------------
// Called at the end of each frame to ensure player and enemy sprites appear
// above bullets and other game elements that may have overdrawn them.
// -----------------------------------------------------------------------------
void redrawEntities(Application* app_p, Graphics_Context* g_sContext,
                    int playerSize, int enemySize) {
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

// =============================================================================
// Application_handlePlayScreen — Main Gameplay Loop
// =============================================================================
// Called every super-loop cycle while in PLAY_SCREEN. On first entry, draws
// the full game screen and initializes bullets. Every subsequent cycle:
//   1. Checks game over — exits early if either HP has reached zero
//   2. Updates player movement, game timer, enemy movement
//   3. Manages enemy attack patterns and updates all enemy bullets
//   4. Handles player shooting and updates all player bullets
//   5. Redraws player and enemy on top of all other graphics
//
// All game parameters are defined as named constants at the top for easy tuning.
// =============================================================================
void Application_handlePlayScreen(Application* app_p, HAL* hal_p) {
    static Graphics_Context g_sContext;

    // -------------------------------------------------------------------------
    // Game Constants — Tune these to adjust gameplay feel
    // -------------------------------------------------------------------------
    const int playerSize              = 8,   screenWidth  = 128, screenHeight = 128;
    const int moveSpeed               = 5,   moveUpdateFrequency = 70, topBoundary = 30;
    const int bulletSpeed             = 6,   bulletLength = 4,   bulletUpdateFrequency = 100;
    const int enemySize               = 10,  enemySpeed   = 2,   enemyUpdateFrequency  = 80;
    const int hpCircleRadius          = 11,  hpCircleX    = 12,  hpCircleY    = screenHeight - 12;
    const int hpCircleArea            = 35;
    const int enemyHpCircleRadius     = 12,  enemyHpCircleX = screenWidth - 14, enemyHpCircleY = 11;
    const int enemyHpCircleArea       = 35;
    const int enemyBulletSpeed        = 5,   enemyBulletUpdateFrequency = 80, enemyBulletLength = 4;
    const int patternSpawnInterval    = 30,  attackCooldown = 200;

    // -------------------------------------------------------------------------
    // First Entry: Draw Full Screen and Reset Bullet State
    // -------------------------------------------------------------------------
    if (!app_p->PlayScreenDrawn) {
        drawInitialGameScreen(&g_sContext, app_p, hpCircleX, hpCircleY, hpCircleRadius,
                              enemyHpCircleX, enemyHpCircleY, enemyHpCircleRadius,
                              playerSize, enemySize);

        int i;
        for (i = 0; i < MAX_BULLETS; i++) {
            app_p->bullets[i].active       = false;
            app_p->bullets[i].frameCounter = 0;
        }
        app_p->moveFrameCounter  = 0;
        app_p->enemyFrameCounter = 0;
        app_p->PlayScreenDrawn   = true;
    }

    // Check game over first — stop all updates if true
    if (handleGameOver(&g_sContext, app_p, hal_p)) return;

    // -------------------------------------------------------------------------
    // Update All Gameplay Systems Each Frame
    // -------------------------------------------------------------------------
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

    // Redraw entities last so they appear above bullets
    redrawEntities(app_p, &g_sContext, playerSize, enemySize);
}
