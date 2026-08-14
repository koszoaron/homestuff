/*
 * Pin map and timing constants -- the one place to edit hardware details.
 * Nothing else in the codebase contains a bare pin number.
 *
 * Keep this in step with the pin table in CLAUDE.md; changing a pin here
 * without updating that table is explicitly out of bounds.
 */

#pragma once

#include <stdint.h>

/*
 * --- Float sensor inputs ---------------------------------------------
 *
 * Wired active LOW: the internal pull-up (plus an external 10k) holds the
 * pin high, and the float switch closes it to ground when raised. That
 * polarity is inverted exactly once, in readFloat() -- see main.cpp.
 */
constexpr uint8_t PIN_FLOAT_MID = 2;   /* PD2, DIP 4 */
constexpr uint8_t PIN_FLOAT_TOP = 3;   /* PD3, DIP 5 */

/* --- Mirror outputs to the downstream ESP32-C6 (active HIGH) --------- */
constexpr uint8_t PIN_OUT_MID = 6;     /* PD6, DIP 12 */
constexpr uint8_t PIN_OUT_TOP = 7;     /* PD7, DIP 13 */

/* --- Indicator LEDs (active HIGH, each with a series resistor) -------- */
constexpr uint8_t PIN_LED_GREEN  = 8;  /* PB0, DIP 14 */
constexpr uint8_t PIN_LED_YELLOW = 9;  /* PB1, DIP 15 */
constexpr uint8_t PIN_LED_RED    = 10; /* PB2, DIP 16 */

/* --- Timing ---------------------------------------------------------- */

/*
 * Deliberately generous: float switches slosh and chatter. If the field
 * behaviour is still twitchy, raise this rather than inventing a cleverer
 * algorithm.
 */
constexpr uint16_t DEBOUNCE_MS = 50;

/* RED toggles every BLINK_MS while in FAULT, giving a 2 Hz blink. */
constexpr uint16_t BLINK_MS = 250;

/*
 * Long enough for the input RC filters (10k + 100nF, ~1 ms) to charge
 * through the pull-ups before the first reading is trusted.
 */
constexpr uint16_t INPUT_SETTLE_MS = 10;
