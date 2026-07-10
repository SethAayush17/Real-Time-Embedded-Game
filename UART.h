// =============================================================================
// UART.h — UART Serial Communication Driver Header
// =============================================================================
// Defines the UART struct, hardware pin mappings, baud rate options, and
// function prototypes for serial communication on the MSP432 via eUSCI_A.
//
// The UART struct encapsulates all hardware configuration needed to control
// a specific UART peripheral:
//   - config:          Communication settings (baud rate, parity, stop bits,
//                      data length, bit order, clock source, oversampling)
//   - moduleInstance:  Which eUSCI_A module to use (e.g. EUSCI_A0_BASE)
//   - port:            GPIO port for UART TX/RX pins
//   - pins:            GPIO pin mask for TX/RX on the selected port
//
// Typical usage sequence:
//   1. UART_construct(moduleInstance, port, pins) — configure GPIO and params
//   2. UART_SetBaud_Enable(uart, baudChoice)      — set baud rate and enable
//   3. UART_hasChar(uart)                         — poll before reading
//   4. UART_getChar(uart)                         — read one received character
//   5. UART_canSend(uart)                         — check before transmitting
//   6. UART_sendChar(uart, c)                     — transmit one character
//
// All communication functions are non-blocking, consistent with the
// super-loop architecture used throughout the application.
//
// Note on API compatibility:
//   The UART config struct was renamed from eUSCI_UART_Config to
//   eUSCI_UART_ConfigV1 in a 2019 DriverLib update. If compilation fails,
//   uncomment the alternate #define below and comment out the active one.
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#ifndef HAL_UART_H_
#define HAL_UART_H_

#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// =============================================================================
// DriverLib Compatibility
// Uncomment the alternate define if compilation fails due to API version mismatch
// =============================================================================
// #define UART_Config eUSCI_UART_Config   // Pre-2019 DriverLib
#define UART_Config eUSCI_UART_ConfigV1    // Post-2019 DriverLib (current)

// =============================================================================
// USB UART Hardware Pin Mappings
// =============================================================================
// Defines the UART module connected to the host PC via USB through the
// onboard XDS110 debug probe. Used for serial terminal communication.

#define USB_UART_PORT       GPIO_PORT_P1
#define USB_UART_PINS       (GPIO_PIN2 | GPIO_PIN3)  // P1.2 = TX, P1.3 = RX
#define USB_UART_INSTANCE   EUSCI_A0_BASE

// =============================================================================
// Baud Rate Enumeration
// =============================================================================
// Supported baud rates for UART communication.
// Used as an index into the lookup tables in UART_SetBaud_Enable().
// NUM_BAUD_CHOICES must remain the last entry — it defines the table size.

enum _UART_Baudrate {
    BAUD_9600,          // 9,600 bits per second
    BAUD_19200,         // 19,200 bits per second
    BAUD_38400,         // 38,400 bits per second
    BAUD_57600,         // 57,600 bits per second
    NUM_BAUD_CHOICES    // Total number of supported baud rates (do not remove)
};
typedef enum _UART_Baudrate UART_Baudrate;

// =============================================================================
// UART Struct
// =============================================================================
// Encapsulates all hardware configuration for one UART peripheral instance.
// Pass a pointer to this struct into all UART_ functions after construction.
//
// Treat all members as PRIVATE — only access from functions prefixed "UART_".

struct _UART {
    UART_Config config;         // Full eUSCI_A communication configuration
    uint32_t moduleInstance;    // eUSCI_A module base address (e.g. EUSCI_A0_BASE)
    uint32_t port;              // GPIO port for UART TX/RX pins
    uint32_t pins;              // GPIO pin bitmask for TX and RX
};
typedef struct _UART UART;

// =============================================================================
// Function Prototypes
// =============================================================================

// ----------------------------------------------------------------------------
// Configuration Functions
// ----------------------------------------------------------------------------

/** Constructs a UART object — configures GPIO pins and communication parameters */
UART UART_construct(uint32_t moduleInstance, uint32_t port, uint32_t pins);

/** Sets baud rate using lookup tables and enables the UART module */
void UART_SetBaud_Enable(UART* uart_p, UART_Baudrate baudrate);

/** Updates the baud rate on an already-enabled UART instance */
void UART_updateBaud(UART* uart_p, UART_Baudrate baudChoice);

// ----------------------------------------------------------------------------
// Communication Functions
// ----------------------------------------------------------------------------

/** Returns true if a character is available in the receive buffer */
bool UART_hasChar(UART* uart_p);

/**
 * Reads and returns one character from the receive buffer.
 * Call UART_hasChar() first — reading an empty buffer returns undefined data.
 */
char UART_getChar(UART* uart_p);

/** Returns true if the transmit buffer is ready to accept a character */
bool UART_canSend(UART* uart_p);

/**
 * Transmits one character — non-blocking, silently drops if buffer not ready.
 * Call UART_canSend() first if guaranteed delivery is required.
 */
void UART_sendChar(UART* uart_p, char c);

#endif /* HAL_UART_H_ */
