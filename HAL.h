// =============================================================================
// HAL.h — Hardware Abstraction Layer (HAL) Header
// =============================================================================
// Defines the top-level HAL struct that aggregates all hardware peripherals
// on the MSP432 LaunchPad and BoosterPack into a single object.
//
// The HAL cleanly separates hardware interaction from application logic —
// all GPIO reads, LED writes, button debouncing, and UART communication
// go through this layer so the application code never touches hardware
// registers directly.
//
// Peripherals included:
//   - 4 LaunchPad LEDs (LED1 red, LED2 red/blue/green)
//   - 3 BoosterPack LEDs (red, blue, green)
//   - 5 Buttons (LaunchPad S1/S2, BoosterPack S1/S2, joystick button)
//   - 1 UART instance for serial communication
//
// Usage:
//   1. Create exactly ONE HAL instance per project using HAL_construct()
//   2. Store it inside the main Application struct so all functions have access
//   3. Call HAL_refresh() once per main loop cycle to update all button states
//
// WARNING: You should have exactly ONE HAL struct in your entire project.
// Multiple HAL instances will cause conflicts in GPIO and peripheral state.
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#ifndef HAL_HAL_H_
#define HAL_HAL_H_

#include <HAL/Button.h>
#include <HAL/LED.h>
#include <HAL/Timer.h>
#include <HAL/UART.h>
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// =============================================================================
// HAL Struct
// Aggregates all hardware peripherals into a single object passed through
// the application. One instance of this struct represents the entire board.
// =============================================================================
struct _HAL {

    // -------------------------------------------------------------------------
    // LEDs — LaunchPad
    // LED1 is single-color (red only)
    // LED2 is RGB — one LED struct per color channel
    // -------------------------------------------------------------------------
    LED launchpadLED1;          // LaunchPad LED1 — red only
    LED launchpadLED2Red;       // LaunchPad LED2 — red channel
    LED launchpadLED2Blue;      // LaunchPad LED2 — blue channel
    LED launchpadLED2Green;     // LaunchPad LED2 — green channel

    // -------------------------------------------------------------------------
    // LEDs — BoosterPack
    // RGB LED — one struct per color channel
    // -------------------------------------------------------------------------
    LED boosterpackRed;         // BoosterPack LED — red channel
    LED boosterpackBlue;        // BoosterPack LED — blue channel
    LED boosterpackGreen;       // BoosterPack LED — green channel

    // -------------------------------------------------------------------------
    // Buttons — LaunchPad
    // -------------------------------------------------------------------------
    Button launchpadS1;         // LaunchPad S1 button
    Button launchpadS2;         // LaunchPad S2 button

    // -------------------------------------------------------------------------
    // Buttons — BoosterPack
    // JS = joystick button (press down on the joystick)
    // -------------------------------------------------------------------------
    Button boosterpackS1;       // BoosterPack S1 button (BB1)
    Button boosterpackS2;       // BoosterPack S2 button (BB2)
    Button boosterpackJS;       // BoosterPack joystick button (JSB)

    // -------------------------------------------------------------------------
    // UART — Serial Communication
    // -------------------------------------------------------------------------
    UART uart;                  // UART instance for serial communication
};
typedef struct _HAL HAL;

// =============================================================================
// Function Prototypes
// =============================================================================

/** Constructs the HAL by initializing all peripheral member structs */
HAL HAL_construct();

/** Refreshes all input peripherals — call exactly once per main loop cycle */
void HAL_refresh(HAL* api);

#endif /* HAL_HAL_H_ */
