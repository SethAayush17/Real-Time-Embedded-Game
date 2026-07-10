// =============================================================================
// Timer.h — Hardware and Software Timer Header
// =============================================================================
// Defines system clock constants, the SWTimer struct, and function prototypes
// for the software timer system built on top of TIMER32_0_BASE.
//
// All software timers (SWTimer) derive their timing from a single hardware
// timer running at SYSTEM_CLOCK Hz. The hardware timer runs continuously and
// generates a rollover interrupt each time it counts down to zero. SWTimers
// record the rollover count and counter value at start time, then compute
// elapsed cycles by comparing against current values.
//
// System Clock:
//   SYSTEM_CLOCK = 48 MHz — changing this affects ALL timing across the board.
//   Any module that depends on timing should #include this header and reference
//   the SYSTEM_CLOCK #define rather than hardcoding a frequency.
//
// SWTimer Usage:
//   1. SWTimer_construct(waitTime_ms) — create a timer with a millisecond duration
//   2. SWTimer_start()               — begin timing (must call before any checks)
//   3. SWTimer_expired()             — returns true once wait period has elapsed
//   4. SWTimer_start() again         — restart the timer if needed
//
// WARNING: Treat all SWTimer struct members as PRIVATE. Never access
// cyclesToWait, startCounter, or startRollovers directly from outside Timer.c.
// Only use functions prefixed with "SWTimer_".
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#ifndef HAL_TIMER_H_
#define HAL_TIMER_H_

#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

// =============================================================================
// Timing Constants
// =============================================================================

#define MS_DIVISION_FACTOR  1000        // Milliseconds per second (for ms → cycles conversion)
#define US_DIVISION_FACTOR  1000000     // Microseconds per second (for us → cycles conversion)

// =============================================================================
// System Clock Configuration
// =============================================================================
// SYSTEM_CLOCK defines the DCO frequency for the entire board.
// Changing this value affects ALL modules that reference this header.
// Must match the value passed to CS_setDCOFrequency() in InitSystemTiming().

#define SYSTEM_CLOCK    48000000    // 48 MHz system clock
#define LOADVALUE       0xFFFFFFFF  // Maximum 32-bit timer load value (full period)
#define PRESCALER       1           // Hardware timer prescaler (no division)

// =============================================================================
// SWTimer Struct
// =============================================================================
// Encapsulates all state for a single software timer instance.
// Multiple SWTimer instances can run simultaneously — all share the same
// underlying TIMER32_0_BASE hardware timer.
//
// Treat all members as PRIVATE — only access from functions prefixed "SWTimer_".
// Do not read or write members directly from outside Timer.c.
//
// A timer must be constructed before starting, and started before any
// elapsed-time or expiry functions are called.

struct _SWTimer {
    uint64_t cyclesToWait;      // Number of hardware cycles to wait before expiration
    uint32_t startCounter;      // Hardware counter value captured at SWTimer_start()
    uint32_t startRollovers;    // Rollover count captured at SWTimer_start()
};
typedef struct _SWTimer SWTimer;

// =============================================================================
// Function Prototypes
// =============================================================================

/**
 * Initializes the system clock to SYSTEM_CLOCK Hz and starts TIMER32_0_BASE
 * as the hardware reference timer for all SWTimer instances.
 * Call immediately after disabling the Watchdog timer in main().
 * DO NOT MODIFY without fully understanding flash wait state requirements.
 */
void InitSystemTiming();

/** Constructs a SWTimer with the given wait duration in milliseconds (not yet started) */
SWTimer SWTimer_construct(uint64_t waitTime_ms);

/** Starts (or restarts) a constructed SWTimer — must be called before expiry checks */
void SWTimer_start(SWTimer* timer);

/**
 * Internal helper — returns hardware cycles elapsed since the timer was started.
 * You do not need to call this directly outside of Timer.c.
 */
uint64_t SWTimer_elapsedCycles(SWTimer* timer);

/** Returns true if the timer's wait period has fully elapsed, false otherwise */
bool SWTimer_expired(SWTimer* timer);

#endif /* HAL_TIMER_H_ */
