/*
 * UART.h
 *
 *  Created on: Dec 31, 2019
 *      Author: Matthew Zhong
 *  Supervisor: Leyla Nazhand-Ali
 *
 *  Description: UART (Universal Asynchronous Receiver/Transmitter) driver
 *               for serial communication on the MSP432 microcontroller.
 *
 *  UART Struct Explanation:
 *  The UART struct contains all hardware configuration information needed
 *  to control a specific UART peripheral for serial communication on the MSP432.
 *  It stores four key pieces of data:
 *    - config: Detailed communication settings (baud rate, parity, stop bits, etc.)
 *    - moduleInstance: Which physical UART module to use (e.g., EUSCI_A0_BASE)
 *    - port: Which GPIO port the UART pins are connected to
 *    - pins: Which specific pins on that port to configure for UART
 *
 *  The struct interacts with UART functions in a consistent pattern:
 *    1. UART_construct() takes hardware parameters and assgins the struct
 *       members data, creating a complete UART object
 *    2. All other UART functions accept a pointer to this struct and read its
 *       members to determine which hardware peripheral to interact with and
 *       how to configure it
 */

#ifndef HAL_UART_H_
#define HAL_UART_H_

#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// Ever since the API update in mid 2019, the UART configuration struct's
// name changed from [eUSCI_UART_Config] to [eUSCI_UART_ConfigV1]. If your code
// does not compile, uncomment the alternate #define and comment the original
// one. This should not really matter anymore
// -----------------------------------------------------------------------------
// #define UART_Config eUSCI_UART_Config
#define UART_Config eUSCI_UART_ConfigV1

// USB UART configuration macros
// These define the UART module connected to USB on Launchpad via XDS110
#define USB_UART_PORT GPIO_PORT_P1
#define USB_UART_PINS (GPIO_PIN2 | GPIO_PIN3)  // Pins for UART TX and RX
#define USB_UART_INSTANCE EUSCI_A0_BASE

// Enumeration of supported baud rates for UART communication
// These are the standard rates that UART_construct() can use during initialization
enum _UART_Baudrate {
  BAUD_9600,          // 9600 bits per second
  BAUD_19200,         // 19200 bits per second
  BAUD_38400,         // 38400 bits per second
  BAUD_57600,         // 57600 bits per second
  NUM_BAUD_CHOICES    // Total number of baud rate options
};
typedef enum _UART_Baudrate UART_Baudrate;

// UART structure containing all configuration and hardware information
struct _UART {
  UART_Config config;        // Communication settings (baud, parity, etc.)
  uint32_t moduleInstance;   // UART module to use (e.g., EUSCI_A0_BASE)
  uint32_t port;            // GPIO port for UART pins
  uint32_t pins;            // GPIO pins for TX/RX
};
typedef struct _UART UART;

// ----------------------------------------------------------------------------
// UART Configuration Functions
// ----------------------------------------------------------------------------

// Constructs a UART object with specified hardware parameters
// Returns a configured UART struct ready for use
UART UART_construct(uint32_t moduleInstance, uint32_t port, uint32_t pins);

// Sets the baud rate and enables the UART peripheral
void UART_SetBaud_Enable(UART* uart_p, UART_Baudrate baudrate);

// Updates the UART to use a new baud rate
void UART_updateBaud(UART* uart_p, UART_Baudrate baudChoice);

// ----------------------------------------------------------------------------
// UART Communication Functions
// ----------------------------------------------------------------------------

// Checks if a character is available in the receive buffer
// Returns true if data can be read, false otherwise
bool UART_hasChar(UART* uart_p);

// Reads and returns the next character from the receive buffer
// Should only be called after UART_hasChar() returns true
char UART_getChar(UART* uart_p);

// Checks if the transmit buffer is ready to send a character
// Returns true if UART is ready to transmit, false otherwise
bool UART_canSend(UART* uart_p);

// Transmits a single character through UART
// Should only be called after UART_canSend() returns true
void UART_sendChar(UART* uart_p, char c);

#endif /* HAL_UART_H_ */
