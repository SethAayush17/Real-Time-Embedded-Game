// =============================================================================
// LED.c — GPIO LED Driver
// =============================================================================
// Implements control functions for individual LEDs mapped to GPIO output pins
// on the MSP432 LaunchPad and BoosterPack.
//
// Each LED is represented as a struct tracking its port, pin, and current
// lit state. All hardware writes go through DriverLib GPIO calls — the isLit
// flag mirrors the hardware state in software so the current state can be
// queried without re-reading the GPIO register.
//
// Available operations: turn on, turn off, toggle, query state.
//
// To determine which port and pin target which LED, consult the LaunchPad
// Quick Reference Guide and BoosterPack User Manual.
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#include <HAL/LED.h>
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// =============================================================================
// LED_construct — Initialize a GPIO Output Pin as an LED
// =============================================================================
// Configures the given port and pin as a GPIO output, sets it LOW (off),
// and returns an initialized LED struct with isLit = false.
//
// @param port  The GPIO port targeting the desired LED
// @param pin   The GPIO pin targeting the desired LED
// @return      A fully constructed LED struct initialized to the off state
// =============================================================================
LED LED_construct(uint8_t port, uint16_t pin) {
    LED led;

    led.isLit = false;  // LED starts off
    led.port  = port;
    led.pin   = pin;

    // Configure pin as GPIO output and set LOW (off) at startup
    GPIO_setAsOutputPin(led.port, led.pin);
    GPIO_setOutputLowOnPin(led.port, led.pin);

    return led;
}

// =============================================================================
// LED_turnOn — Set LED Output HIGH
// =============================================================================
// Drives the GPIO pin HIGH to illuminate the LED and updates isLit flag.
//
// @param led  Pointer to the LED to turn on
// =============================================================================
void LED_turnOn(LED* led) {
    uint8_t port  = led->port;
    uint16_t pin  = led->pin;

    led->isLit = true;
    GPIO_setOutputHighOnPin(port, pin);
}

// =============================================================================
// LED_turnOff — Set LED Output LOW
// =============================================================================
// Drives the GPIO pin LOW to extinguish the LED and updates isLit flag.
//
// @param led  Pointer to the LED to turn off
// =============================================================================
void LED_turnOff(LED* led) {
    uint8_t port  = led->port;
    uint16_t pin  = led->pin;

    led->isLit = false;
    GPIO_setOutputLowOnPin(port, pin);
}

// =============================================================================
// LED_toggle — Invert LED Output State
// =============================================================================
// Toggles the GPIO pin output and inverts the isLit flag.
// If the LED was on it turns off; if it was off it turns on.
//
// @param led  Pointer to the LED to toggle
// =============================================================================
void LED_toggle(LED* led) {
    uint8_t port  = led->port;
    uint16_t pin  = led->pin;

    led->isLit = !led->isLit;
    GPIO_toggleOutputOnPin(port, pin);
}

// =============================================================================
// LED_isLit — Query Current LED State
// =============================================================================
// Returns the current lit state of the LED based on the software-tracked
// isLit flag — does not re-read the GPIO register directly.
//
// @param led  Pointer to the LED to query
// @return     true if the LED is currently on, false if off
// =============================================================================
bool LED_isLit(LED* led) {
    return led->isLit;
}
