// =============================================================================
// Timer.c — Hardware and Software Timer Implementation
// =============================================================================
// Implements system clock initialization and a software timer (SWTimer) system
// built on top of a single hardware timer (TIMER32_0_BASE).
//
// Architecture:
//   One hardware timer (TIMER32_0_BASE) runs continuously as a 32-bit periodic
//   countdown timer at the full system clock frequency. Each time it rolls over
//   (counts down to zero), an ISR increments a global rollover counter
//   (hwTimerRollovers). All SWTimers derive their timing from this counter —
//   they record the rollover count and hardware counter value at start time,
//   then compute elapsed cycles by comparing against the current values.
//
// This design allows an unlimited number of independent SWTimer instances to
// run simultaneously using only one hardware timer peripheral.
//
// System Clock:
//   InitSystemTiming() configures the DCO to SYSTEM_CLOCK Hz and routes it
//   to MCLK, HSMCLK, and SMCLK. Flash wait states MUST be set before changing
//   the clock frequency — omitting this will brick the board.
//
// SWTimer Usage:
//   1. Call SWTimer_construct(waitTime_ms) to create a timer
//   2. Call SWTimer_start() to begin timing
//   3. Poll SWTimer_expired() to check if the wait time has elapsed
//   4. Optionally use SWTimer_elapsedTimeUS() or SWTimer_percentElapsed()
//      for progress-based timing
//
// WARNING: Do NOT call T32_INT1_IRQHandler() directly — it is an ISR invoked
// automatically by hardware. Calling it manually will corrupt rollover counts
// and break all software timers in the system.
//
// Originally authored by Matthew Zhong.
// =============================================================================

#include <HAL/Timer.h>

// =============================================================================
// Hardware Timer Rollover Counter
// Incremented by the TIMER32_0_BASE ISR on every timer expiration (rollover)
// Used by all SWTimer instances to compute elapsed time
// Declared volatile since it is modified inside an ISR
// =============================================================================
static volatile uint64_t hwTimerRollovers = 0;

// =============================================================================
// T32_INT1_IRQHandler — Hardware Timer Rollover ISR
// =============================================================================
// Automatically invoked by hardware each time TIMER32_0_BASE counts down to
// zero and reloads. Increments the global rollover counter and clears the
// interrupt flag so the next rollover can be detected.
//
// WARNING: Do NOT call this function directly. Manual invocation will corrupt
// the rollover count and break all software timers in the system.
// =============================================================================
void T32_INT1_IRQHandler() {
    hwTimerRollovers++;
    Timer32_clearInterruptFlag(TIMER32_0_BASE);
}

// =============================================================================
// InitSystemTiming — Configure System Clock and Initialize Hardware Timer
// =============================================================================
// Sets the DCO to SYSTEM_CLOCK Hz, configures all clock signals, and starts
// the TIMER32_0_BASE hardware timer that underlies all SWTimer instances.
//
// CRITICAL: Flash wait states must be set to 2 BEFORE calling
// CS_setDCOFrequency(). Omitting this step causes the board to fetch
// instructions before flash memory has settled, producing garbage reads
// and rendering the board unflashable without a factory reset.
//
// Call this function immediately after disabling the Watchdog timer in main().
// DO NOT MODIFY this function unless you fully understand the consequences —
// incorrect clock configuration can brick the board.
// =============================================================================
void InitSystemTiming() {
    // Disable all interrupts before reconfiguring the system clock
    Interrupt_disableMaster();

    // -------------------------------------------------------------------------
    // Set flash wait states BEFORE changing clock frequency
    // Required to prevent instruction fetch failures at higher clock speeds
    // Failure to do this WILL brick the board and require a factory reset
    // -------------------------------------------------------------------------
    FlashCtl_setWaitState(FLASH_BANK0, 2);
    FlashCtl_setWaitState(FLASH_BANK1, 2);

    // Set DCO to the user-specified system clock frequency
    CS_setDCOFrequency(SYSTEM_CLOCK);

    // Route DCO to MCLK, HSMCLK, and SMCLK with no prescaling
    // Route REFOCLK to ACLK (low-frequency auxiliary clock)
    CS_initClockSignal(CS_MCLK,   CS_DCOCLK_SELECT,  CS_CLOCK_DIVIDER_1);
    CS_initClockSignal(CS_HSMCLK, CS_DCOCLK_SELECT,  CS_CLOCK_DIVIDER_1);
    CS_initClockSignal(CS_SMCLK,  CS_DCOCLK_SELECT,  CS_CLOCK_DIVIDER_1);
    CS_initClockSignal(CS_ACLK,   CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    // -------------------------------------------------------------------------
    // Initialize TIMER32_0_BASE as a 32-bit periodic countdown timer
    // Prescaler of 1 maximizes resolution while minimizing ISR frequency
    // LOADVALUE sets the maximum rollover period
    // -------------------------------------------------------------------------
    Timer32_initModule(TIMER32_0_BASE, TIMER32_PRESCALER_1, TIMER32_32BIT,
                       TIMER32_PERIODIC_MODE);
    Timer32_setCount(TIMER32_0_BASE, LOADVALUE);

    // Start the hardware timer in free-running (non-one-shot) mode
    Timer32_startTimer(TIMER32_0_BASE, false);

    // Re-enable interrupts and enable the TIMER32_0_BASE rollover interrupt
    Interrupt_enableMaster();
    Interrupt_enableInterrupt(INT_T32_INT1);
}

// =============================================================================
// SWTimer_construct — Create a Software Timer with a Millisecond Wait Time
// =============================================================================
// Computes the number of hardware counter cycles corresponding to waitTime_ms
// and stores it in the SWTimer struct. The timer is NOT started by construction
// — you must call SWTimer_start() before calling SWTimer_expired() or any
// elapsed-time functions.
//
// @param waitTime_ms  Duration in milliseconds this timer measures before expiry
// @return             A constructed SWTimer (not yet started)
// =============================================================================
SWTimer SWTimer_construct(uint64_t waitTime_ms) {
    SWTimer timer;

    timer.startCounter  = 0;
    timer.startRollovers = 0;

    // Compute cycles per millisecond from clock frequency and prescaler
    uint64_t counterClock       = SYSTEM_CLOCK / PRESCALER;
    uint64_t cyclesPerMillisecond = counterClock / MS_DIVISION_FACTOR;
    timer.cyclesToWait = cyclesPerMillisecond * waitTime_ms;

    return timer;
}

// =============================================================================
// SWTimer_start — Record Start Time for a Software Timer
// =============================================================================
// Captures the current hardware counter value and rollover count as the timer's
// reference point. Call this before any elapsed-time or expiry checks.
// Can also be used to restart a timer that has already expired.
//
// @param timer_p  Pointer to the SWTimer to start
// =============================================================================
void SWTimer_start(SWTimer* timer_p) {
    timer_p->startCounter  = Timer32_getValue(TIMER32_0_BASE);
    timer_p->startRollovers = hwTimerRollovers;
}

// =============================================================================
// SWTimer_elapsedCycles — Compute Hardware Cycles Elapsed Since Timer Start
// =============================================================================
// Internal helper used by SWTimer_expired(), SWTimer_elapsedTimeUS(), and
// SWTimer_percentElapsed(). Accounts for hardware timer rollovers by tracking
// the rollover count delta since the timer was started.
//
// If the timer was never started, returns cycles elapsed since program start.
//
// @param timer_p  Pointer to the target SWTimer
// @return         Number of hardware counter cycles elapsed since start
// =============================================================================
uint64_t SWTimer_elapsedCycles(SWTimer* timer_p) {
    uint64_t rollovers      = hwTimerRollovers - timer_p->startRollovers;
    uint64_t startCounter   = timer_p->startCounter;
    uint64_t currentCounter = Timer32_getValue(TIMER32_0_BASE);

    // Elapsed = (rollovers * full timer period) + (start value - current value)
    // Subtraction works because the timer counts DOWN from LOADVALUE to 0
    uint64_t elapsedCycles = (rollovers * (LOADVALUE + 1)) + startCounter - currentCounter;

    return elapsedCycles;
}

// =============================================================================
// SWTimer_expired — Check if the Timer's Wait Period Has Elapsed
// =============================================================================
// Returns true once the elapsed cycle count meets or exceeds cyclesToWait.
// Does not stop or reset the timer — continues returning true until restarted.
//
// @param timer_p  Pointer to the target SWTimer
// @return         true if wait period has elapsed, false otherwise
// =============================================================================
bool SWTimer_expired(SWTimer* timer_p) {
    uint64_t elapsedCycles = SWTimer_elapsedCycles(timer_p);
    return elapsedCycles >= timer_p->cyclesToWait;
}

// =============================================================================
// SWTimer_elapsedTimeUS — Get Elapsed Time in Microseconds
// =============================================================================
// Returns elapsed time in microseconds since the timer was started.
// Uses microseconds rather than milliseconds for higher precision in
// time-sensitive calculations.
//
// If the timer was never started, returns microseconds since program start.
//
// @param timer_p  Pointer to the target SWTimer
// @return         Microseconds elapsed since the timer was started
// =============================================================================
uint64_t SWTimer_elapsedTimeUS(SWTimer* timer_p) {
    uint64_t elapsedCycles = SWTimer_elapsedCycles(timer_p);

    uint64_t counterClock        = SYSTEM_CLOCK / PRESCALER;
    uint64_t cyclesPerMicrosecond = counterClock / US_DIVISION_FACTOR;

    return elapsedCycles / cyclesPerMicrosecond;
}

// =============================================================================
// SWTimer_percentElapsed — Get Fractional Progress Toward Timer Expiration
// =============================================================================
// Returns a value between 0.0 and 1.0 representing how much of the wait
// period has elapsed. Useful for progress bars or proportional animations.
//
// Returns 1.0 if the timer has expired or was never started (cyclesToWait == 0).
// Clamps to 1.0 if elapsed cycles exceed cyclesToWait.
//
// @param timer_p  Pointer to the target SWTimer
// @return         Progress fraction in range [0.0, 1.0]
// =============================================================================
double SWTimer_percentElapsed(SWTimer* timer_p) {
    // Guard against division by zero — treat as instantly expired
    if (timer_p->cyclesToWait == 0) {
        return 1.0;
    }

    uint64_t elapsedCycles = SWTimer_elapsedCycles(timer_p);
    double result = (double)elapsedCycles / (double)timer_p->cyclesToWait;

    // Clamp to 1.0 — timer cannot exceed 100% elapsed
    return (result > 1.0) ? 1.0 : result;
}
