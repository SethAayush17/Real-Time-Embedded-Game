// =============================================================================
// LED.h — GPIO LED Driver Header
// =============================================================================
// Defines the LED struct, hardware pin mappings, and function prototypes
// for controlling individual LEDs on the MSP432 LaunchPad and BoosterPack.
//
// Each LED is represented as a struct tracking its port, pin, and current
// lit state (isLit). The isLit flag mirrors the hardware output in software
// so the current state can be queried without re-reading the GPIO register.
//
// Usage:
//   1. Construct an LED with LED_construct(port, pin)
//   2. Control it with LED_turnOn(), LED_turnOff(), or LED_toggle()
//   3. Query its state with LED_isLit()
//
// WARNING: Treat all LED struct members as PRIVATE. Never access port, pin,
// or isLit directly from outside this HAL — always use the LED_ functions.
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#ifndef HAL_LED_H_
#define HAL_LED_H_

#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// =============================================================================
// Hardware Pin Mappings
// =============================================================================
// Port and pin definitions for all LEDs on the LaunchPad and BoosterPack.
// Consult the LaunchPad User Guide and BoosterPack User Guide for board layout.

// LaunchPad LED1 — single color (red only)
#define LAUNCHPAD_LED1_PORT             GPIO_PORT_P1
#define LAUNCHPAD_LED1_PIN              GPIO_PIN0

// LaunchPad LED2 — RGB (one port/pin pair per color channel)
#define LAUNCHPAD_LED2_RED_PORT         GPIO_PORT_P2
#define LAUNCHPAD_LED2_RED_PIN          GPIO_PIN0

#define LAUNCHPAD_LED2_GREEN_PORT       GPIO_PORT_P2
#define LAUNCHPAD_LED2_GREEN_PIN        GPIO_PIN1

#define LAUNCHPAD_LED2_BLUE_PORT        GPIO_PORT_P2
#define LAUNCHPAD_LED2_BLUE_PIN         GPIO_PIN2

// BoosterPack LED — RGB (one port/pin pair per color channel)
#define BOOSTERPACK_LED_RED_PORT        GPIO_PORT_P2
#define BOOSTERPACK_LED_RED_PIN         GPIO_PIN6

#define BOOSTERPACK_LED_GREEN_PORT      GPIO_PORT_P2
#define BOOSTERPACK_LED_GREEN_PIN       GPIO_PIN4

#define BOOSTERPACK_LED_BLUE_PORT       GPIO_PORT_P5
#define BOOSTERPACK_LED_BLUE_PIN        GPIO_PIN6

// =============================================================================
// LED Struct
// =============================================================================
// Encapsulates all state for a single GPIO-controlled LED.
// Treat all members as PRIVATE — only access them from functions prefixed
// with "LED_". Do not read or write members directly from outside this HAL.

struct _LED {
    uint8_t port;   // GPIO port this LED is mapped to
    uint16_t pin;   // GPIO pin this LED is mapped to
    bool isLit;     // Software-tracked lit state — mirrors hardware output
};
typedef struct _LED LED;

// =============================================================================
// Function Prototypes
// =============================================================================

/** Constructs and initializes an LED object for the given port and pin (starts off) */
LED LED_construct(uint8_t port, uint16_t pin);

/** Turns the LED on — sets GPIO pin HIGH and updates isLit */
void LED_turnOn(LED* led);

/** Turns the LED off — sets GPIO pin LOW and updates isLit */
void LED_turnOff(LED* led);

/** Toggles the LED — inverts GPIO pin output and isLit flag */
void LED_toggle(LED* led);

/** Returns true if the LED is currently on, false if off */
bool LED_isLit(LED* led);

#endif /* HAL_LED_H_ */
