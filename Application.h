// =============================================================================
// application.h — Main Game Application Header
// =============================================================================
// Defines all data structures, constants, and function prototypes for the
// Touhou dodge game running on an MSP432 microcontroller.
//
// The application is built around a non-blocking super-loop architecture —
// Application_loop() is called repeatedly from main() and uses if statements
// rather than while loops to ensure no single operation ever stalls the processor.
//
// Game structure:
//   - Five screen states managed by a FSM: Title, Menu, Instructions,
//     High Scores, and Play
//   - Player controlled via analog joystick with up to 6 simultaneous bullets
//   - Enemy moves left/right and cycles through 4 distinct attack patterns,
//     spawning up to 20 bullets simultaneously on screen
//   - Score calculated as player HP minus elapsed game time
//   - Top 5 scores stored in a persistent high scores array
//
// All hardware interactions go through the HAL layer (HAL/HAL.h), cleanly
// separating hardware and application logic.
// =============================================================================

#ifndef APPLICATION_H_
#define APPLICATION_H_

#include <HAL/HAL.h>

// =============================================================================
// Bullet Limits
// =============================================================================

// We can have up to 6 bullets from the player on screen at once
#define MAX_BULLETS 6

// The enemy can have up to 20 bullets on screen (needs more for attack patterns)
#define MAX_ENEMY_BULLETS 20

// =============================================================================
// Data Structures
// =============================================================================

// This is what an enemy bullet looks like - it has position, direction, and if it's active
typedef struct {
    int x;                  // Where the bullet is left/right
    int y;                  // Where the bullet is up/down
    int directionX;         // Which way it moves left/right (-1=left, 0=still, 1=right)
    int directionY;         // Which way it moves up/down (-1=up, 0=still, 1=down)
    bool isHorizontal;      // Is it a horizontal line or vertical line?
    bool active;            // Is the bullet actually on the screen right now?
    int frameCounter;       // Helps control how fast the bullet moves
} EnemyBullet;

// This is what a player bullet looks like - simpler than enemy bullets
// Player bullets only go straight up so we don't need as much info
typedef struct {
    int x;                  // Where the bullet is left/right
    int y;                  // Where the bullet is up/down
    bool active;            // Is it currently on screen?
    int frameCounter;       // Controls bullet speed
} Bullet;

// =============================================================================
// Screen State Machine
// =============================================================================

// These are all the different screens in the game
typedef enum {
    TITLE_SCREEN,           // The opening screen with the game title
    MENU_SCREEN,            // Where you pick what to do (play, instructions, high scores)
    PLAY_SCREEN,            // The actual game where you play
    INSTRUCTIONS_SCREEN,    // Tells you how to play
    HIGH_SCORES_SCREEN      // Shows the top 5 scores
} ScreenState;

// =============================================================================
// Main Application Structure
// Holds all game state — passed by pointer through every function in the app
// =============================================================================

struct _Application {
    // -------------------------------------------------------------------------
    // Screen State Flags
    // Each "drawn" flag prevents unnecessary redraws by tracking whether the
    // current screen has already been rendered to the LCD
    // -------------------------------------------------------------------------
    ScreenState currentScreen;          // Which screen are we showing right now?
    bool titleScreenDrawn;              // Did we already draw the title screen?
    SWTimer titleTimer;                 // Timer to know how long to show the title
    bool menuScreenDrawn;               // Did we already draw the menu?
    int menuSelection;                  // Which option is highlighted in the menu?
    bool instructionsScreenDrawn;       // Did we already draw the instructions?
    bool HighScoresScreenDrawn;         // Did we already draw the high scores?
    bool PlayScreenDrawn;               // Did we set up the game screen yet?
    bool gameOverScreenDrawn;           // Did we already show the game over message?

    // -------------------------------------------------------------------------
    // Player State
    // -------------------------------------------------------------------------
    int highScores[5];                  // Array of the top 5 scores
    int playerX;                        // Player's position left/right
    int playerY;                        // Player's position up/down
    int playerHP;                       // Player's health (0-100)
    Bullet bullets[MAX_BULLETS];        // All the player's bullets
    int moveFrameCounter;               // Controls how fast the player moves
    bool shootButtonWasPressed;         // Did we press the shoot button last frame?

    // -------------------------------------------------------------------------
    // Enemy State
    // -------------------------------------------------------------------------
    int enemyX;                         // Enemy's position left/right
    int enemyY;                         // Enemy's position up/down
    int enemyHP;                        // Enemy's health (0-100)
    int enemyDirection;                 // Which way the enemy is moving (-1=left, 1=right)
    int enemyFrameCounter;              // Controls how fast the enemy moves

    // -------------------------------------------------------------------------
    // Enemy Attack Patterns
    // The enemy cycles through 4 attack patterns (0-3), each spawning bullets
    // in a different direction — vertical rain, horizontal from left,
    // diagonal from corners, and horizontal from both sides
    // -------------------------------------------------------------------------
    EnemyBullet enemyBullets[MAX_ENEMY_BULLETS];  // All the enemy's bullets
    int currentAttackPattern;           // Which attack pattern is happening (0-3)
    int patternFrameCounter;            // Controls when to spawn the next bullet in pattern
    int attackCooldownCounter;          // Wait time between attack patterns
    bool patternActive;                 // Is an attack pattern happening right now?

    // -------------------------------------------------------------------------
    // Game Timer
    // Tracks elapsed seconds during gameplay — subtracted from player HP for score
    // -------------------------------------------------------------------------
    int gameTimer;                      // How many seconds have passed in the game
    int timerFrameCounter;              // Counts frames to know when a second passes
};

typedef struct _Application Application;

// =============================================================================
// Function Prototypes
// =============================================================================

// ----------------------------------------------------------------------------
// Core Application Functions
// ----------------------------------------------------------------------------

// Sets up the game when it first starts
Application Application_construct();

// This runs over and over - it's the main game loop
void Application_loop(Application* app, HAL* hal);

// ----------------------------------------------------------------------------
// Screen Handlers
// Each function manages one screen state — called from Application_loop()
// based on the current value of app->currentScreen
// ----------------------------------------------------------------------------

// Shows the title screen
void Application_handleTitleScreen(Application* app_p, HAL* hal_p);

// Shows the menu where you pick options
void Application_handleMenuScreen(Application* app_p, HAL* hal_p);

// Shows how to play the game
void Application_handleInstructionsScreen(Application* app_p, HAL* hal_p);

// Shows the top 5 high scores
void Application_handleHighScoresScreen(Application* app_p, HAL* hal_p);

// Runs the actual game
void Application_handlePlayScreen(Application* app_p, HAL* hal_p);

#endif /* APPLICATION_H_ */
