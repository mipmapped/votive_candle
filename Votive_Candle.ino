#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/power.h>

/*
AI Requirement:
Write me some arduino code for attiny85. This uses three pins. PB0 is connected to a candle LED such that an output of 1 is on and 0, or set to input, it will be off. PB4 is an input connected to a tilt sensor and shall have an internal pullup. When tilted the pin will be low. PB3 shall be an input with an internal pullup. It will be connected to a reed relay such that when the relay is activated the pin will be 0.

This design shall be optimized for low power and use sleep mode with WDT for timed sleeping. PB4 shall be used to wake from any sleep state.

The code has three major states: LIT_TIMER, LIT_SHORT_TIMER and DEEP_SLEEP.

When powered up the candle shall be on and enter the LIT_TIMER state.

When the enters the LIT_TIMER state. It shall remain in this state for approximately 2 hours, at which time it will exit the state and turn off the candle. It shall use the WDT and wake states to do the timing and keep batter usage low. When in the state for greater than 0.5s and a transitions from untilted to tilted is sensed (as an interrupt) it will exit the state and turn the candle off.

The code shall enter the DEEP_SLEEP state when the candle becomes unlit. In this state the candle pin shall change to an input to save power. When it has been in this state for more than 0.5s and the tilt state transitions from untilted to tilted (as an interrupt) it shall exit the state, turn on the candle and enter the LIT_TIMER state.

If it is in the DEEP_SLEEP state and PB3 transitions low or high it shall turn the candle on and enter the LIT_SHORT_TIMER state. This state is similar to the LIT_TIMER state except that the time in this state is approximately 5 seconds. Entering this state shall light the candle. When the timer expires and the state exits the candle shall turn off and it shall enter the DEEP_SLEEP state.

Claude Sonet 4 Repsonse:
I'll create Arduino code that implements your three-state candle controller with the reed relay functionality on PB3.This code implements your three-state candle controller with reed relay functionality. Here are the key features:
Three States:
	• LIT_TIMER: Candle on for 2 hours, tilt after 0.5s turns off
	• LIT_SHORT_TIMER: Candle on for 5 seconds, tilt after 0.5s turns off
	• DEEP_SLEEP: Candle off (pin as input), responds to tilt (>0.5s) or reed transitions
State Transitions:
	1. Power-up: Candle on → LIT_TIMER state
	2. LIT_TIMER: 
		○ Timeout (2hrs) OR tilt (>0.5s) → candle off → DEEP_SLEEP
	3. LIT_SHORT_TIMER: 
		○ Timeout (5s) OR tilt (>0.5s) → candle off → DEEP_SLEEP
	4. DEEP_SLEEP: 
		○ Tilt (>0.5s) → candle on → LIT_TIMER
		○ Reed transition (any time) → candle on → LIT_SHORT_TIMER
Reed Relay Features:
	• PB3 with internal pullup: Pin reads LOW when relay activated
	• Any transition triggers: Both low→high and high→low activate the short timer
	• Immediate response: No 0.5s delay requirement for reed transitions
	• Priority handling: Reed transitions take precedence in DEEP_SLEEP state
Key Technical Features:
	• Pin change interrupts: Both PB4 (tilt) and PB3 (reed) wake from sleep
	• Edge detection: Tilt only responds to untilted→tilted (HIGH→LOW)
	• 0.5s protection: Uses stateTimeCounter to prevent immediate tilt responses
	• Power optimization: 
		○ Deep sleep with WDT timing
		○ Candle pin becomes input when off
		○ Disabled unused peripherals
Operation Flow:
	1. Normal cycle: LIT_TIMER (2hrs) ↔ DEEP_SLEEP
	2. Reed activation: DEEP_SLEEP → LIT_SHORT_TIMER (5s) → DEEP_SLEEP
	3. Tilt shortcuts: Can exit any lit state early (after 0.5s)
	4. Low power: WDT wake-up, deep sleep between cycles
The reed relay provides a convenient way to get a quick 5-second light without entering the full 2-hour cycle.
*/

// Pin definitions
#define CANDLE_PIN PB0
#define TILT_PIN PB4
#define REED_PIN PB3

// States
enum State {
  LIT_TIMER,       // Long timed lit state (2 hours)
  LIT_SHORT_TIMER, // Short timed lit state (5 seconds)
  DEEP_SLEEP       // Deep sleep state
};

// Global variables
volatile State currentState = LIT_TIMER;
volatile bool tiltTransitionDetected = false;
volatile bool reedTransitionDetected = false;
volatile bool previousTiltState = HIGH;       // Track previous state for edge detection
volatile bool previousReedState = HIGH;       // Track previous reed state
volatile uint16_t wdtCounter = 0;
volatile uint16_t stateTimeCounter = 0;       // Track time in current state

// Timing constants
const uint16_t LIT_TIMEOUT_SECONDS = 7200;    // 2 hours
const uint16_t LIT_SHORT_TIMEOUT_SECONDS = 5; // 5 seconds
const uint16_t MIN_STATE_TIME_SECONDS = 1;    // 0.5s minimum (using 1s WDT, so 1 cycle minimum)
const uint8_t WDT_CYCLES_PER_SECOND = 1;      // Using 1s WDT timeout

// WDT timeout tracking
const uint16_t LIT_WDT_CYCLES = LIT_TIMEOUT_SECONDS * WDT_CYCLES_PER_SECOND;
const uint16_t LIT_SHORT_WDT_CYCLES = LIT_SHORT_TIMEOUT_SECONDS * WDT_CYCLES_PER_SECOND;
const uint16_t MIN_STATE_WDT_CYCLES = MIN_STATE_TIME_SECONDS * WDT_CYCLES_PER_SECOND;

void setup() {
  // Disable unused peripherals to save power
  ADCSRA = 0;  // Disable ADC
  power_adc_disable();
  power_timer1_disable();
  
  // Configure pins
  pinMode(CANDLE_PIN, OUTPUT);
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(REED_PIN, INPUT_PULLUP);
  
  // Turn on candle initially and enter LIT_TIMER
  digitalWrite(CANDLE_PIN, HIGH);
  
  // Initialize state tracking
  previousTiltState = digitalRead(TILT_PIN);
  previousReedState = digitalRead(REED_PIN);
  
  // Setup pin change interrupts for both tilt sensor (PB4) and reed relay (PB3)
  GIMSK |= (1 << PCIE);     // Enable pin change interrupts
  PCMSK |= (1 << PCINT4);   // Enable interrupt on PB4 (tilt)
  PCMSK |= (1 << PCINT3);   // Enable interrupt on PB3 (reed)
  
  // Initialize state - candle is lit, so enter LIT_TIMER
  currentState = LIT_TIMER;
  wdtCounter = 0;
  stateTimeCounter = 0;
  
  // Enable interrupts
  sei();
  
  // Setup and start watchdog timer
  setupWDT();
}

void loop() {
  switch (currentState) {
    case LIT_TIMER:
      handleLitTimerState();
      break;
      
    case LIT_SHORT_TIMER:
      handleLitShortTimerState();
      break;
      
    case DEEP_SLEEP:
      handleDeepSleepState();
      break;
  }
}

void handleLitTimerState() {
  // Check if tilt transition detected and we've been in state > 0.5s
  if (tiltTransitionDetected && stateTimeCounter >= MIN_STATE_WDT_CYCLES) {
    tiltTransitionDetected = false;
    turnOffCandle();
    enterDeepSleepState();
    return;
  }
  
  // Clear transition flags if we haven't been in state long enough
  if (tiltTransitionDetected && stateTimeCounter < MIN_STATE_WDT_CYCLES) {
    tiltTransitionDetected = false;
  }
  
  // Clear any reed transitions (not used in this state)
  reedTransitionDetected = false;
  
  // Check if lit timeout reached (2 hours) - turn off and enter deep sleep
  if (wdtCounter >= LIT_WDT_CYCLES) {
    turnOffCandle();
    enterDeepSleepState();
    return;
  }
  
  // Go to sleep and wait for WDT or interrupt
  enterLightSleep();
}

void handleLitShortTimerState() {
  // Check if tilt transition detected and we've been in state > 0.5s
  if (tiltTransitionDetected && stateTimeCounter >= MIN_STATE_WDT_CYCLES) {
    tiltTransitionDetected = false;
    turnOffCandle();
    enterDeepSleepState();
    return;
  }
  
  // Clear transition flags if we haven't been in state long enough
  if (tiltTransitionDetected && stateTimeCounter < MIN_STATE_WDT_CYCLES) {
    tiltTransitionDetected = false;
  }
  
  // Clear any reed transitions (not used in this state)
  reedTransitionDetected = false;
  
  // Check if short lit timeout reached (5 seconds) - turn off and enter deep sleep
  if (wdtCounter >= LIT_SHORT_WDT_CYCLES) {
    turnOffCandle();
    enterDeepSleepState();
    return;
  }
  
  // Go to sleep and wait for WDT or interrupt
  enterLightSleep();
}

void handleDeepSleepState() {
  // Check if reed transition detected - turn on candle and enter LIT_SHORT_TIMER
  if (reedTransitionDetected) {
    reedTransitionDetected = false;
    turnOnCandle();
    enterLitShortTimerState();
    return;
  }
  
  // Check if tilt transition detected and we've been in state > 0.5s
  if (tiltTransitionDetected && stateTimeCounter >= MIN_STATE_WDT_CYCLES) {
    tiltTransitionDetected = false;
    turnOnCandle();
    enterLitTimerState();
    return;
  }
  
  // Clear transition flag if we haven't been in state long enough
  if (tiltTransitionDetected && stateTimeCounter < MIN_STATE_WDT_CYCLES) {
    tiltTransitionDetected = false;
  }
  
  // Stay in deep sleep (no timeout for this state)
  enterDeepSleep();
}

void turnOnCandle() {
  pinMode(CANDLE_PIN, OUTPUT);
  digitalWrite(CANDLE_PIN, HIGH);
}

void turnOffCandle() {
  digitalWrite(CANDLE_PIN, LOW);
}

void enterLitTimerState() {
  currentState = LIT_TIMER;
  wdtCounter = 0;          // Reset lit timer
  stateTimeCounter = 0;    // Reset state time counter
  setupWDT();              // Enable WDT for timing
}

void enterLitShortTimerState() {
  currentState = LIT_SHORT_TIMER;
  wdtCounter = 0;          // Reset short lit timer
  stateTimeCounter = 0;    // Reset state time counter
  setupWDT();              // Enable WDT for timing
}

void enterDeepSleepState() {
  currentState = DEEP_SLEEP;
  stateTimeCounter = 0;    // Reset state time counter
  setupWDT();              // Keep WDT running for state time tracking
}

void enterDeepSleep() {
  // Change candle pin to input to save power (candle is off)
  pinMode(CANDLE_PIN, INPUT);
  
  // Update pin states before sleeping
  previousTiltState = digitalRead(TILT_PIN);
  previousReedState = digitalRead(REED_PIN);
  
  // Enter deepest sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
}

void enterLightSleep() {
  // Update pin states before sleeping
  previousTiltState = digitalRead(TILT_PIN);
  previousReedState = digitalRead(REED_PIN);
  
  // Use power-down mode (WDT will still wake us up)
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
}

void setupWDT() {
  // Configure WDT for 1 second timeout
  cli();
  wdt_reset();
  
  // Set WDT to interrupt mode with 1s timeout
  WDTCR |= (1 << WDCE) | (1 << WDE);
  WDTCR = (1 << WDIE) | (1 << WDP2) | (1 << WDP1);  // 1 second timeout
  
  sei();
}

// Pin change interrupt service routine (tilt sensor and reed relay)
ISR(PCINT0_vect) {
  // Read current pin states
  bool currentTiltState = digitalRead(TILT_PIN);
  bool currentReedState = digitalRead(REED_PIN);
  
  // Check for tilt transition from untilted (HIGH) to tilted (LOW)
  if (previousTiltState == HIGH && currentTiltState == LOW) {
    tiltTransitionDetected = true;
  }
  
  // Check for reed transition (any change - low to high or high to low)
  if (previousReedState != currentReedState) {
    reedTransitionDetected = true;
  }
  
  // Update previous states
  previousTiltState = currentTiltState;
  previousReedState = currentReedState;
}

// Watchdog timer interrupt service routine
ISR(WDT_vect) {
  // Always increment state time counter
  stateTimeCounter++;
  
  // Increment appropriate timer based on current state
  if (currentState == LIT_TIMER || currentState == LIT_SHORT_TIMER) {
    wdtCounter++;
  }
  
  // Update pin state tracking after wake-up
  previousTiltState = digitalRead(TILT_PIN);
  previousReedState = digitalRead(REED_PIN);
  
  // WDT interrupt automatically wakes from sleep
}