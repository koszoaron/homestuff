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
 * The switches are normally CLOSED with the float resting in its downward
 * position and open once water lifts it. Against the internal pull-up plus
 * an external 10k that gives:
 *
 *     pin LOW  = switch ON  = float down = water below this sensor
 *     pin HIGH = switch OFF = float up   = water has reached this sensor
 *
 * The pin level becomes "true = switch ON" exactly once, in readSwitchOn()
 * -- see main.cpp. Note that a switch being ON means the tank is *empty* at
 * that height, which is why LOW-LOW is the empty tank and not the full one.
 *
 * Consequence worth knowing: a cut sensor cable leaves the pin pulled up,
 * which reads the same as a raised float, so the tank reports fuller than
 * it is. That is the safe direction here, so it is deliberate rather than
 * something to "fix".
 */
constexpr uint8_t PIN_FLOAT_MID = 2;   /* PD2, DIP 4 */
constexpr uint8_t PIN_FLOAT_TOP = 3;   /* PD3, DIP 5 */

/*
 * --- Mirror outputs to the downstream ESP32-C6 -----------------------
 *
 * Each pin repeats the level of its matching input pin: LOW while the
 * switch is ON, HIGH once water has raised the float. The inversion the C6
 * expects is supplied by the BUFFER between the two chips, so it must not
 * be done here as well -- inverting in both places cancels out. Both lines
 * therefore idle LOW at the ATmega on an empty tank. Nothing is
 * special-cased here, the FAULT combination included.
 */
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
