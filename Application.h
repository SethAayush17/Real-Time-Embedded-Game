#ifndef APPLICATION_H_
#define APPLICATION_H_

#include <HAL/HAL.h>

// We can have up to 6 bullets from the player on screen at once
#define MAX_BULLETS 6

// The enemy can have up to 20 bullets on screen (needs more for attack patterns)
#define MAX_ENEMY_BULLETS 20

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

// These are all the different screens in the game
typedef enum {
    TITLE_SCREEN,           // The opening screen with the game title
    MENU_SCREEN,            // Where you pick what to do (play, instructions, high scores)
    PLAY_SCREEN,            // The actual game where you play
    INSTRUCTIONS_SCREEN,    // Tells you how to play
    HIGH_SCORES_SCREEN      // Shows the top 5 scores
} ScreenState;

// This is what a player bullet looks like - simpler than enemy bullets
// Player bullets only go straight up so we don't need as much info
typedef struct {
    int x;                  // Where the bullet is left/right
    int y;                  // Where the bullet is up/down
    bool active;            // Is it currently on screen?
    int frameCounter;       // Controls bullet speed
} Bullet;

// This is the main structure that holds everything about the game
struct _Application {
    // Variables about which screen we're on
    ScreenState currentScreen;          // Which screen are we showing right now?
    bool titleScreenDrawn;              // Did we already draw the title screen?
    SWTimer titleTimer;                 // Timer to know how long to show the title
    bool menuScreenDrawn;               // Did we already draw the menu?
    int menuSelection;                  // Which option is highlighted in the menu?
    bool instructionsScreenDrawn;       // Did we already draw the instructions?
    bool HighScoresScreenDrawn;         // Did we already draw the high scores?
    bool PlayScreenDrawn;               // Did we set up the game screen yet?
    bool gameOverScreenDrawn;           // Did we already show the game over message?

    // Player variables
    int highScores[5];                  // Array of the top 5 scores
    int playerX;                        // Player's position left/right
    int playerY;                        // Player's position up/down
    int playerHP;                       // Player's health (0-100)
    Bullet bullets[MAX_BULLETS];        // All the player's bullets
    int moveFrameCounter;               // Controls how fast the player moves
    bool shootButtonWasPressed;         // Did we press the shoot button last frame?

    // Enemy variables
    int enemyX;                         // Enemy's position left/right
    int enemyY;                         // Enemy's position up/down
    int enemyHP;                        // Enemy's health (0-100)
    int enemyDirection;                 // Which way the enemy is moving (-1=left, 1=right)
    int enemyFrameCounter;              // Controls how fast the enemy moves

    // Enemy attack variables
    EnemyBullet enemyBullets[MAX_ENEMY_BULLETS];  // All the enemy's bullets
    int currentAttackPattern;           // Which attack pattern is happening (0-3)
    int patternFrameCounter;            // Controls when to spawn the next bullet in pattern
    int attackCooldownCounter;          // Wait time between attack patterns
    bool patternActive;                 // Is an attack pattern happening right now?

    // Timer stuff
    int gameTimer;                      // How many seconds have passed in the game
    int timerFrameCounter;              // Counts frames to know when a second passes
};

typedef struct _Application Application;

// ----------------------------------------------------------------------------
// Main game functions
// ----------------------------------------------------------------------------

// Sets up the game when it first starts
Application Application_construct();

// This runs over and over - it's the main game loop
void Application_loop(Application* app, HAL* hal);

// ----------------------------------------------------------------------------
// Functions for each screen
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
