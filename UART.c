/*
 * UART.c
 *
 *  Created on: Dec 31, 2019
 *      Author: Matthew Zhong
 *  Supervisor: Leyla Nazhand-Ali
 *
 *  Description: Implementation file for UART driver on MSP432.
 *               Provides functions for configuring UART communication,
 *               checking status, and transmitting/receiving data.
 */

#include <HAL/Timer.h>
#include <HAL/UART.h>

// ----------------------------------------------------------------------------
// UART Construction and Configuration
// ----------------------------------------------------------------------------


//Constructs and initializes a UART object with the specified hardware parameters.

UART UART_construct(uint32_t moduleInstance, uint32_t port, uint32_t pins) {
  // Create the UART struct
  UART uart;

  // Store hardware configuration parameters
  uart.moduleInstance = moduleInstance;
  uart.port = port;
  uart.pins = pins;

  // Configure GPIO pins for UART peripheral function
  GPIO_setAsPeripheralModuleFunctionInputPin(uart.port, uart.pins,
                                             GPIO_PRIMARY_MODULE_FUNCTION);

  // Configure UART communication parameters (excluding baudrate)
  uart.config.parity = EUSCI_A_UART_NO_PARITY;              // No parity bit
  uart.config.msborLsbFirst = EUSCI_A_UART_LSB_FIRST;       // Send LSB first
  uart.config.numberofStopBits = EUSCI_A_UART_ONE_STOP_BIT; // 1 stop bit
  uart.config.uartMode = EUSCI_A_UART_MODE;                 // Standard UART mode
  uart.config.dataLength = EUSCI_A_UART_8_BIT_LEN;          // 8 data bits

  // Return the configured UART instance (ready for baudrate setting)
  return uart;
}


 // Sets the baudrate and enables the UART module for communication.

void UART_SetBaud_Enable(UART* uart_p, UART_Baudrate baudChoice) {
  // Use system clock (SMCLK at 48MHz) for baudrate generation
  uart_p->config.selectClockSource = EUSCI_A_UART_CLOCKSOURCE_SMCLK;

  // Enable oversampling mode for all supported baudrates (N>16)
  uart_p->config.overSampling = EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION;

  // Lookup tables for baudrate configuration
  // Each column corresponds to: 9600, 19200, 38400, 57600 BPS
  uint32_t clockPrescalerMapping[NUM_BAUD_CHOICES] = {312, 156, 78, 52};
  uint32_t firstModRegMapping[NUM_BAUD_CHOICES] = {8, 4, 2, 1};
  uint32_t secondModRegMapping[NUM_BAUD_CHOICES] = {0x00, 0x00, 0x00, 0x25};

  // Set baudrate parameters using lookup tables
  uart_p->config.clockPrescalar = clockPrescalerMapping[baudChoice];
  uart_p->config.firstModReg = firstModRegMapping[baudChoice];
  uart_p->config.secondModReg = secondModRegMapping[baudChoice];

  // Initialize UART hardware with configuration and enable the module
  UART_initModule(uart_p->moduleInstance, &uart_p->config);
  UART_enableModule(uart_p->moduleInstance);
}

// ----------------------------------------------------------------------------
// UART Status and Data Functions
// ----------------------------------------------------------------------------


//Checks if a character is available in the UART receive buffer.

bool UART_hasChar(UART* uart_p) {
  // Query the receive interrupt flag status
  uint8_t interruptStatus = UART_getInterruptStatus(
      uart_p->moduleInstance, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG);

  // Return true if receive flag is set (character available)
  return (interruptStatus == EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG);
}


// Retrieves a character from the UART receive buffer.


char UART_getChar(UART* uart_p) {
  return UART_receiveData(uart_p->moduleInstance);
}


//Checks if the UART transmit buffer is ready to send a character.

bool UART_canSend(UART* uart_p) {
  // Query the transmit interrupt flag status
  uint8_t interruptStatus = UART_getInterruptStatus(
      uart_p->moduleInstance, EUSCI_A_UART_TRANSMIT_INTERRUPT_FLAG);

  // Return true if transmit flag is set (buffer ready)
  return (interruptStatus == EUSCI_A_UART_TRANSMIT_INTERRUPT_FLAG);
}


 // Transmits a character through UART (non-blocking).

void UART_sendChar(UART* uart_p, char c) {
  // Only send if transmit buffer is ready
  if (UART_canSend(uart_p)) {
    UART_transmitData(uart_p->moduleInstance, c);
  }
}
