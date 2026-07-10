// =============================================================================
// Button.c — GPIO Pushbutton Driver with Debouncing FSM
// =============================================================================
// Implements a software-debounced pushbutton using a 4-state FSM to filter
// out electrical noise (contact bounce) that occurs when a physical button
// is pressed or released.
//
// Without debouncing, a single button press can register as multiple rapid
// transitions as the contacts physically bounce — this FSM waits for the
// signal to remain stable for DEBOUNCE_TIME_MS before accepting a state change.
//
// FSM States:
//   StableR      — Button is stably released (default state)
//   TransitionRP — Button appears pressed but timer hasn't expired yet
//   StableP      — Button is stably pressed (confirmed after debounce timer)
//   TransitionPR — Button appears released but timer hasn't expired yet
//
// Outputs:
//   pushState — current debounced press state (PRESSED or RELEASED)
//   isTapped  — true for exactly one refresh cycle when a press is first
//               detected (rising edge detection — was RELEASED, now PRESSED)
//
// Originally authored by Matthew Zhong, supervised by Leyla Nazhand-Ali.
// =============================================================================

#include <HAL/Button.h>

// =============================================================================
// Button_construct — Initialize a GPIO Pushbutton with Debouncing FSM
// =============================================================================
// Configures the given port and pin as an input with a pull-up resistor,
// then initializes all debouncing FSM variables to their stable released state.
//
// Pull-up resistor note: All buttons on the board can safely use a pull-up
// resistor — a double pull-up has no impact on input voltage. This means
// the pin reads HIGH (RELEASED) by default and LOW (PRESSED) when pushed.
//
// @param port  The GPIO port used to initialize this button
// @param pin   The GPIO pin used to initialize this button
// @return      A fully constructed Button with debouncing FSM initialized
// =============================================================================
Button Button_construct(uint8_t port, uint16_t pin) {
  // The button object which will be returned at the end of construction
  Button button;

  // Initialize the member variables for port and pin of the button.
  button.port = port;
  button.pin = pin;

  // Configure pin as input with internal pull-up resistor
  // Pin reads HIGH when released, LOW when pressed
  GPIO_setAsInputPinWithPullUpResistor(port, pin);

  // Initialize debouncing FSM to stable released state
  button.debounceState = StableR;
  button.timer = SWTimer_construct(DEBOUNCE_TIME_MS);
  SWTimer_start(&button.timer);

  // Initialize buffered outputs — button starts unpressed and untapped
  button.pushState = RELEASED;
  button.isTapped = false;

  return button;
}

// =============================================================================
// Button_isPressed — Get Current Debounced Press State
// =============================================================================
// Returns whether the button is currently held down based on the last
// refreshed pushState. Does NOT poll GPIO directly or advance the FSM —
// call Button_refresh() first to update the internal state.
//
// @param button  The Button object to query
// @return        true if button is currently pressed, false if released
// =============================================================================
bool Button_isPressed(Button* button) { return button->pushState == PRESSED; }

// =============================================================================
// Button_isTapped — Get Rising Edge Tap Detection
// =============================================================================
// Returns whether the button was tapped (rising edge) since the last refresh.
// A tap is true for exactly one refresh cycle — the cycle where the button
// transitioned from RELEASED to PRESSED. Does NOT advance the FSM.
//
// @param button  The Button object to query
// @return        true if button was just tapped, false otherwise
// =============================================================================
bool Button_isTapped(Button* button) { return button->isTapped; }

// =============================================================================
// Button_refresh — Advance Debouncing FSM and Update Outputs
// =============================================================================
// Polls the GPIO pin for the raw button state and advances the 4-state
// debouncing FSM by one step. Should be called once per main loop iteration.
//
// The FSM filters out contact bounce by requiring the signal to remain
// stable for DEBOUNCE_TIME_MS before accepting a state transition.
// Erroneous signals during transition states reset the timer rather than
// causing false state changes.
//
// @param button  The Button object to refresh
// =============================================================================
void Button_refresh(Button* button) {
  // Retrieve the port and pin targets
  uint8_t port = button->port;
  uint16_t pin = button->pin;

  // Poll raw GPIO value — may contain bounce noise, handled by FSM below
  uint16_t rawButtonStatus = GPIO_getInputPinValue(port, pin);
  int newPushState = RELEASED;

  // -------------------------------------------------------------------------
  // Debouncing FSM — 4 states
  // -------------------------------------------------------------------------
  switch (button->debounceState) {

    // StableR: Button is confirmed released
    // Transition to TransitionRP if raw signal shows pressed
    case StableR:
      if (rawButtonStatus == PRESSED) {
        SWTimer_start(&button->timer); // Start debounce timer
        button->debounceState = TransitionRP;
      }
      newPushState = RELEASED;
      break;

    // StableP: Button is confirmed pressed
    // Transition to TransitionPR if raw signal shows released
    case StableP:
      if (rawButtonStatus == RELEASED) {
        SWTimer_start(&button->timer); // Start debounce timer
        button->debounceState = TransitionPR;
      }
      newPushState = PRESSED;
      break;

    // TransitionRP: Signal appears pressed but not yet confirmed
    // Accept as pressed only after debounce timer expires
    // Reset to StableR if signal bounces back to released before timer expires
    case TransitionRP:
      if (rawButtonStatus == RELEASED) {
        button->debounceState = StableR; // Bounce detected — return to stable released
      } else if (SWTimer_expired(&button->timer)) {
        button->debounceState = StableP; // Timer expired — confirm pressed
      }
      newPushState = RELEASED;
      break;

    // TransitionPR: Signal appears released but not yet confirmed
    // Accept as released only after debounce timer expires
    // Reset to StableP if signal bounces back to pressed before timer expires
    case TransitionPR:
      if (rawButtonStatus == PRESSED) {
        button->debounceState = StableP; // Bounce detected — return to stable pressed
      } else if (SWTimer_expired(&button->timer)) {
        button->debounceState = StableR; // Timer expired — confirm released
      }
      newPushState = PRESSED;
  }

  // -------------------------------------------------------------------------
  // FSM Output Update
  // isTapped is true for exactly one refresh cycle on rising edge
  // (was RELEASED last refresh, is PRESSED this refresh)
  // -------------------------------------------------------------------------
  button->isTapped = newPushState == PRESSED && button->pushState == RELEASED;
  button->pushState = newPushState;
}
