// =============================================================================
// UART.c — UART Serial Communication Driver
// =============================================================================
// Implements UART configuration, status checking, and data transmission/
// reception for the MSP432 using the eUSCI_A peripheral in UART mode.
//
// The driver supports four standard baud rates (9600, 19200, 38400, 57600 BPS)
// configured via lookup tables for the clock prescaler and modulation registers.
// All baud rates use the 48MHz SMCLK source with oversampling enabled (N > 16)
// for improved noise tolerance and timing accuracy.
//
// Communication parameters (fixed):
//   - 8 data bits, 1 stop bit, no parity, LSB first
//   - Standard UART mode (not IrDA or multiprocessor)
//
// Usage:
//   1. UART_construct()         — configure GPIO pins and communication params
//   2. UART_SetBaud_Enable()    — set baud rate and enable the UART module
//   3. UART_hasChar()           — poll for incoming data before reading
//   4. UART_getChar()           — read one character from the receive buffer
//   5. UART_canSend()           — check transmit buffer availability
//   6. UART_sendChar()          — transmit one character (non-blocking)
//
// All send/receive functions are non-blocking — they check flags rather than
// waiting, consistent with the non-blocking super-loop architecture.
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#include <HAL/Timer.h>
#include <HAL/UART.h>

// =============================================================================
// UART_construct — Configure GPIO Pins and Initialize Communication Parameters
// =============================================================================
// Configures the given port and pins as UART peripheral function pins and
// sets all fixed communication parameters (parity, stop bits, data length,
// bit order, UART mode). Does NOT set the baud rate or enable the module —
// call UART_SetBaud_Enable() after construction to complete initialization.
//
// @param moduleInstance  eUSCI_A module instance (e.g. EUSCI_A0_BASE)
// @param port            GPIO port for UART TX/RX pins
// @param pins            GPIO pins for UART TX/RX (bitmask)
// @return                A constructed UART object ready for baud rate config
// =============================================================================
UART UART_construct(uint32_t moduleInstance, uint32_t port, uint32_t pins) {
    UART uart;

    // Store hardware configuration parameters
    uart.moduleInstance = moduleInstance;
    uart.port = port;
    uart.pins = pins;

    // Configure GPIO pins as UART peripheral function (TX/RX)
    GPIO_setAsPeripheralModuleFunctionInputPin(uart.port, uart.pins,
                                               GPIO_PRIMARY_MODULE_FUNCTION);

    // Set fixed communication parameters — same for all baud rates
    uart.config.parity            = EUSCI_A_UART_NO_PARITY;               // No parity bit
    uart.config.msborLsbFirst     = EUSCI_A_UART_LSB_FIRST;               // LSB transmitted first
    uart.config.numberofStopBits  = EUSCI_A_UART_ONE_STOP_BIT;            // 1 stop bit
    uart.config.uartMode          = EUSCI_A_UART_MODE;                    // Standard UART mode
    uart.config.dataLength        = EUSCI_A_UART_8_BIT_LEN;               // 8 data bits per frame

    return uart;
}

// =============================================================================
// UART_SetBaud_Enable — Set Baud Rate and Enable UART Module
// =============================================================================
// Configures the clock source, oversampling mode, and baud rate registers
// using lookup tables indexed by the selected UART_Baudrate enum value,
// then initializes and enables the UART hardware module.
//
// Baud rate lookup tables (indexed by baudChoice, at 48MHz SMCLK):
//   9600  BPS: prescaler=312, firstMod=8,  secondMod=0x00
//   19200 BPS: prescaler=156, firstMod=4,  secondMod=0x00
//   38400 BPS: prescaler=78,  firstMod=2,  secondMod=0x00
//   57600 BPS: prescaler=52,  firstMod=1,  secondMod=0x25
//
// Oversampling is enabled for all rates since N = SMCLK/baudrate > 16
// for all supported rates at 48MHz, which improves noise rejection.
//
// @param uart_p     Pointer to a constructed UART object
// @param baudChoice Desired baud rate selected from UART_Baudrate enum
// =============================================================================
void UART_SetBaud_Enable(UART* uart_p, UART_Baudrate baudChoice) {
    // Use SMCLK (48MHz) as the baud rate clock source
    uart_p->config.selectClockSource = EUSCI_A_UART_CLOCKSOURCE_SMCLK;

    // Enable oversampling mode — required when N = clock/baudrate > 16
    uart_p->config.overSampling = EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION;

    // Baud rate configuration lookup tables (one entry per UART_Baudrate enum value)
    uint32_t clockPrescalerMapping[NUM_BAUD_CHOICES] = {312, 156, 78,   52  };
    uint32_t firstModRegMapping   [NUM_BAUD_CHOICES] = {8,   4,   2,    1   };
    uint32_t secondModRegMapping  [NUM_BAUD_CHOICES] = {0x00, 0x00, 0x00, 0x25};

    // Apply baud rate settings from lookup tables
    uart_p->config.clockPrescalar = clockPrescalerMapping[baudChoice];
    uart_p->config.firstModReg    = firstModRegMapping[baudChoice];
    uart_p->config.secondModReg   = secondModRegMapping[baudChoice];

    // Initialize hardware with full config and enable the UART module
    UART_initModule(uart_p->moduleInstance, &uart_p->config);
    UART_enableModule(uart_p->moduleInstance);
}

// =============================================================================
// UART_hasChar — Check if a Character is Available in the Receive Buffer
// =============================================================================
// Polls the receive interrupt flag to determine if a character has arrived.
// Non-blocking — returns immediately without waiting for data.
//
// @param uart_p  Pointer to the UART object to check
// @return        true if a character is available, false otherwise
// =============================================================================
bool UART_hasChar(UART* uart_p) {
    uint8_t interruptStatus = UART_getInterruptStatus(
        uart_p->moduleInstance, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG);

    return (interruptStatus == EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG);
}

// =============================================================================
// UART_getChar — Read One Character from the Receive Buffer
// =============================================================================
// Retrieves the next available character from the UART receive buffer.
// Call UART_hasChar() first to confirm a character is available — calling
// this function on an empty buffer returns undefined data.
//
// @param uart_p  Pointer to the UART object to read from
// @return        The received character
// =============================================================================
char UART_getChar(UART* uart_p) {
    return UART_receiveData(uart_p->moduleInstance);
}

// =============================================================================
// UART_canSend — Check if the Transmit Buffer is Ready
// =============================================================================
// Polls the transmit interrupt flag to determine if the transmit buffer
// is empty and ready to accept a new character. Non-blocking.
//
// @param uart_p  Pointer to the UART object to check
// @return        true if the transmit buffer is ready, false otherwise
// =============================================================================
bool UART_canSend(UART* uart_p) {
    uint8_t interruptStatus = UART_getInterruptStatus(
        uart_p->moduleInstance, EUSCI_A_UART_TRANSMIT_INTERRUPT_FLAG);

    return (interruptStatus == EUSCI_A_UART_TRANSMIT_INTERRUPT_FLAG);
}

// =============================================================================
// UART_sendChar — Transmit One Character (Non-Blocking)
// =============================================================================
// Sends one character over UART only if the transmit buffer is ready.
// If the buffer is not ready, the character is silently dropped — this
// maintains non-blocking behavior consistent with the super-loop architecture.
// Use UART_canSend() before calling if you need guaranteed delivery.
//
// @param uart_p  Pointer to the UART object to transmit through
// @param c       The character to send
// =============================================================================
void UART_sendChar(UART* uart_p, char c) {
    if (UART_canSend(uart_p)) {
        UART_transmitData(uart_p->moduleInstance, c);
    }
}
