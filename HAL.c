// =============================================================================
// Button.h — GPIO Pushbutton Driver Header
// =============================================================================
// Defines the Button object, debouncing FSM states, hardware pin mappings,
// and function prototypes for the software-debounced pushbutton driver.
//
// The Button object uses a 4-state debouncing FSM to filter contact bounce
// from physical button presses. See Button.c for full FSM implementation.
//
// Logic levels:
//   PRESSED  = 0 (pin pulled LOW when button is pushed, due to pull-up resistor)
//   RELEASED = 1 (pin reads HIGH by default when button is not pushed)
//
// Usage:
//   1. Construct a button with Button_construct(port, pin)
//   2. Call Button_refresh() exactly ONCE per main loop cycle to advance FSM
//   3. Read outputs with Button_isPressed() or Button_isTapped()
//
// WARNING: Call Button_refresh() exactly once per cycle. Calling it multiple
// times per cycle will advance the FSM more than once, causing missed edge
// transitions and incorrect tap/press detection.
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#ifndef HAL_BUTTON_H_
#define HAL_BUTTON_H_

#include <HAL/Timer.h>
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// =============================================================================
// Timing and Logic Level Constants
// =============================================================================

#define DEBOUNCE_TIME_MS 5  // Duration signal must remain stable to confirm state change
#define PRESSED  0          // Pin reads LOW when button is pressed (pull-up configuration)
#define RELEASED 1          // Pin reads HIGH when button is released (pull-up configuration)

// =============================================================================
// Hardware Pin Mappings
// =============================================================================
// Port and pin definitions for all buttons on the LaunchPad and BoosterPack.
// Consult the LaunchPad User Guide and BoosterPack User Guide for board layout.

// LaunchPad onboard buttons
#define LAUNCHPAD_S1_PORT       GPIO_PORT_P1
#define LAUNCHPAD_S1_PIN        GPIO_PIN1

#define LAUNCHPAD_S2_PORT       GPIO_PORT_P1
#define LAUNCHPAD_S2_PIN        GPIO_PIN4

// BoosterPack buttons
#define BOOSTERPACK_S1_PORT     GPIO_PORT_P5
#define BOOSTERPACK_S1_PIN      GPIO_PIN1

#define BOOSTERPACK_S2_PORT     GPIO_PORT_P3
#define BOOSTERPACK_S2_PIN      GPIO_PIN5

// BoosterPack joystick button
#define BOOSTERPACK_JS_PORT     GPIO_PORT_P4
#define BOOSTERPACK_JS_PIN      GPIO_PIN1

// =============================================================================
// Debouncing FSM State Enum
// =============================================================================
// Four states representing stable and transitional conditions for the button
// signal. Transition states use a timer to confirm stability before accepting
// a state change — erroneous signals during transition reset the timer.
//
//   StableR      — Button confirmed released (default state at startup)
//   TransitionRP — Signal appears pressed; waiting for debounce timer to confirm
//   StableP      — Button confirmed pressed
//   TransitionPR — Signal appears released; waiting for debounce timer to confirm

enum _DebounceState { StableP, TransitionPR, TransitionRP, StableR };
typedef enum _DebounceState DebounceState;

// =============================================================================
// Button Struct
// =============================================================================
// Encapsulates all state for a single debounced pushbutton.
// Treat all members as PRIVATE — only access them from functions prefixed
// with "Button_". Do not read or write members directly from outside this HAL.

struct _Button {
    uint8_t port;           // GPIO port this button is mapped to
    uint16_t pin;           // GPIO pin this button is mapped to

    DebounceState debounceState; // Current state of the 4-state debouncing FSM
    SWTimer timer;               // Software timer used to wait out bouncy transitions

    int pushState;          // Debounced output: PRESSED or RELEASED
    bool isTapped;          // Rising edge output: true for one cycle when first pressed
};
typedef struct _Button Button;

// =============================================================================
// Function Prototypes
// =============================================================================

/** Constructs and initializes a Button object for the given port and pin */
Button Button_construct(uint8_t port, uint16_t pin);

/** Returns true if the button is currently held down (debounced) */
bool Button_isPressed(Button* button);

/** Returns true for one cycle when the button transitions from released to pressed */
bool Button_isTapped(Button* button);

/** Advances the debouncing FSM — call exactly once per main loop cycle */
void Button_refresh(Button* button);

#endif /* HAL_BUTTON_H_ */
