#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/power.h>

// Pin definitions
#define CANDLE_PIN PB0
#define TILT_PIN PB4

/*
Alan Simmonds 2025-08-09
All code written by Claude Sonnet 4 AI

Promtp:
Write me some arduino code for attiny85. This uses two pins. PB0 is connected to a candle LED such that an output of 1 is on and 0, or set to input, it will be off. PB4 is an input connected to a tilt sensor and shall have an internal pullup. When tilted the pin will be low. This design shall be optimized for low power and use sleep mode with WDT for timed sleeping. PB4 shall be used to wake from any sleep state.

The code has two major states: LIT_TIMER and DEEP_SLEEP.

When powered up the candle shall be on.

When the candle becomes lit the code shall enter the LIT_TIMER state. It shall remain in this state for approximately 2 hours, at which time it will exit the state and turn off the candle. It shall use the WDT and wake states to do the timing and keep batter usage low. When in the state for greater than 0.5s and a transitions from untilted to tilted is sensed (as an interrupt) it will exit the state and turn the candle off.

The code shall enter the DEEP_SLEEP state when the candle becomes unlit. In this state the candle pin shall change to an input to save power. When it has been in this state for more than 0.5s and the tilt state transitions from untilted to tilted (as an interrupt) it shall exit the state and turn on the candle.

AI output:
I'll create Arduino code that implements your simplified two-state system with the 0.5-second minimum time requirements for tilt responses.This code implements your simplified two-state candle controller with the following key features:
Two Major States:
	• LIT_TIMER: Candle on, 2-hour timeout with WDT timing
	• DEEP_SLEEP: Candle off, candle pin as input for power savings
State Transitions:
	1. Power-up: Candle turns on → enters LIT_TIMER state
	2. LIT_TIMER: 
		○ Runs for 2 hours OR until tilt (if >0.5s in state)
		○ Tilt or timeout → candle off → DEEP_SLEEP state
	3. DEEP_SLEEP: 
		○ Tilt after >0.5s in state → candle on → LIT_TIMER state
Key Features:
	• 0.5-second minimum: Uses stateTimeCounter to track time in current state, requires ≥1 WDT cycle (≥0.5s effective) before responding to tilts
	• Edge detection: Only responds to untilted→tilted transitions (HIGH→LOW)
	• Power optimization: 
		○ Deep sleep with WDT wake-up
		○ Candle pin becomes input in DEEP_SLEEP to eliminate current draw
		○ Disabled ADC and unused peripherals
	• Interrupt-based: Pin change interrupt works in all sleep modes
Operation Flow:
	1. Startup: Candle immediately on, enters LIT_TIMER
	2. Normal cycle: LIT_TIMER (2hrs max) ↔ DEEP_SLEEP (indefinite)
	3. Tilt protection: Ignores tilts for first 0.5s in each state
	4. Low power: Uses WDT for timing, deep sleep between wake-ups
The system is much simpler with just the two core states while maintaining all the power optimization and reliable tilt detection you requested.

*/

// States
enum State {
  LIT_TIMER,     // Timed lit state (2 hours)
  DEEP_SLEEP     // Deep sleep state
};

// Global variables
volatile State currentState = LIT_TIMER;
volatile bool tiltTransitionDetected = false;
volatile bool previousTiltState = HIGH;       // Track previous state for edge detection
volatile uint16_t wdtCounter = 0;
volatile uint16_t stateTimeCounter = 0;       // Track time in current state

// Timing constants
const uint16_t LIT_TIMEOUT_SECONDS = 7200;    // 2 hours
//const uint16_t LIT_TIMEOUT_SECONDS = 20;    // 2 hours
const uint16_t MIN_STATE_TIME_SECONDS = 1;    // 0.5s minimum (using 1s WDT, so 1 cycle minimum)
const uint8_t WDT_CYCLES_PER_SECOND = 1;      // Using 1s WDT timeout

// WDT timeout tracking
const uint16_t LIT_WDT_CYCLES = LIT_TIMEOUT_SECONDS * WDT_CYCLES_PER_SECOND;
const uint16_t MIN_STATE_WDT_CYCLES = MIN_STATE_TIME_SECONDS * WDT_CYCLES_PER_SECOND;

void setup() {
  // Disable unused peripherals to save power
  ADCSRA = 0;  // Disable ADC
  power_adc_disable();
  power_timer1_disable();
  
  // Configure pins
  pinMode(CANDLE_PIN, OUTPUT);
  pinMode(TILT_PIN, INPUT_PULLUP);
  
  // Turn on candle initially (becomes lit)
  digitalWrite(CANDLE_PIN, HIGH);
  
  // Initialize tilt state tracking
  previousTiltState = digitalRead(TILT_PIN);
  
  // Setup pin change interrupt for tilt sensor (PB4)
  GIMSK |= (1 << PCIE);     // Enable pin change interrupts
  PCMSK |= (1 << PCINT4);   // Enable interrupt on PB4
  
  // Initialize state - candle becomes lit, so enter LIT_TIMER
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
  
  // Clear transition flag if we haven't been in state long enough
  if (tiltTransitionDetected && stateTimeCounter < MIN_STATE_WDT_CYCLES) {
    tiltTransitionDetected = false;
  }
  
  // Check if lit timeout reached (2 hours) - turn off and enter deep sleep
  if (wdtCounter >= LIT_WDT_CYCLES) {
    turnOffCandle();
    enterDeepSleepState();
    return;
  }
  
  // Go to sleep and wait for WDT or tilt interrupt
  enterLightSleep();
}

void handleDeepSleepState() {
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

void enterDeepSleepState() {
  currentState = DEEP_SLEEP;
  stateTimeCounter = 0;    // Reset state time counter
  setupWDT();              // Keep WDT running for state time tracking
}

void enterDeepSleep() {
  // Change candle pin to input to save power (candle is off)
  pinMode(CANDLE_PIN, INPUT);
  
  // Update tilt state before sleeping
  previousTiltState = digitalRead(TILT_PIN);
  
  // Enter deepest sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
}

void enterLightSleep() {
  // Update tilt state before sleeping
  previousTiltState = digitalRead(TILT_PIN);
  
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

// Pin change interrupt service routine (tilt sensor)
ISR(PCINT0_vect) {
  // Read current tilt state
  bool currentTiltState = digitalRead(TILT_PIN);
  
  // Check for transition from untilted (HIGH) to tilted (LOW)
  if (previousTiltState == HIGH && currentTiltState == LOW) {
    tiltTransitionDetected = true;
  }
  
  // Update previous state
  previousTiltState = currentTiltState;
}

// Watchdog timer interrupt service routine
ISR(WDT_vect) {
  // Always increment state time counter
  stateTimeCounter++;
  
  // Increment lit timer only when in LIT_TIMER state
  if (currentState == LIT_TIMER) {
    wdtCounter++;
  }
  
  // Update tilt state tracking after wake-up
  previousTiltState = digitalRead(TILT_PIN);
  
  // WDT interrupt automatically wakes from sleep
}