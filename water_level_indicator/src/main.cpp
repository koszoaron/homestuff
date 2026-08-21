/*
 * Water level indicator for a two-float water container.
 *
 * Reads two float switches, debounces them, drives a green/yellow/red LED
 * from the resulting level, and hands the debounced states to a downstream
 * ESP32-C6 as an inverse mirror of the switch inputs. See CLAUDE.md for the
 * behaviour table and the constraints this code works under (Arduino API
 * only, no ISRs, no delay() in loop()).
 */

#include <Arduino.h>
#include <avr/wdt.h>

#include "config.h"

/* LOW and HIGH are Arduino macros, so the enumerators need other names. */
enum class Level : uint8_t { LOW_, MID, HIGH_, FAULT };

/*
 * One per float switch. Plain data, no methods -- see the coding
 * conventions in CLAUDE.md.
 */
struct Debouncer {
    uint8_t pin;
    bool raw;                 /* most recent raw reading */
    bool stable;              /* committed, debounced value */
    unsigned long changedAt;  /* millis() when raw last moved */
};

static Debouncer midFloat;
static Debouncer topFloat;

static unsigned long lastBlinkToggle;
static bool blinkOn;

/*
 * The only place that knows how the sensors are wired. The switches are
 * closed while the float rests down and open as water lifts it, so a
 * pulled-up pin reads LOW while the switch is ON. Everything downstream
 * reads "true = switch ON = water is below this float".
 */
static bool readSwitchOn(uint8_t pin)
{
    return digitalRead(pin) == LOW;
}

static void initDebouncer(Debouncer &d, uint8_t pin)
{
    const bool raw = readSwitchOn(pin);

    d.pin = pin;
    d.raw = raw;
    /*
     * Seed the debounced value from the pin so the LEDs are correct
     * immediately at power-up rather than after the first debounce window.
     */
    d.stable = raw;
    d.changedAt = millis();
}

static void updateDebouncer(Debouncer &d)
{
    const bool raw = readSwitchOn(d.pin);

    if (raw != d.raw) {
        /* Reading moved: restart the stability window. */
        d.raw = raw;
        d.changedAt = millis();
    }

    /*
     * Unsigned subtraction, so this stays correct across the ~49 day
     * millis() rollover. Never write `millis() >= d.changedAt + DEBOUNCE_MS`.
     */
    if (d.raw != d.stable && (millis() - d.changedAt) >= DEBOUNCE_MS) {
        d.stable = d.raw;
    }
}

/*
 * Free-running so the blink phase needs no extra state. FAULT is rare
 * enough that starting mid-phase does not matter.
 */
static void updateBlink()
{
    if ((millis() - lastBlinkToggle) >= BLINK_MS) {
        lastBlinkToggle = millis();
        blinkOn = !blinkOn;
    }
}

static Level levelFrom(bool midOn, bool topOn)
{
    if (topOn) {
        /* Top float still down, so the water is below it. */
        return midOn ? Level::LOW_ : Level::MID;
    }

    /*
     * Top float raised. The middle one must be raised as well; a middle
     * switch that is still ON says the water cannot have reached the top,
     * so a sensor or its wiring has failed.
     */
    return midOn ? Level::FAULT : Level::HIGH_;
}

static void applyOutputs(Level level, bool midOn, bool topOn)
{
    bool green = false;
    bool yellow = false;
    bool red = false;

    switch (level) {
    case Level::LOW_:
        green = true;
        break;

    case Level::MID:
        yellow = true;
        break;

    case Level::HIGH_:
        red = true;
        break;

    case Level::FAULT:
        red = blinkOn;
        break;
    }

    digitalWrite(PIN_LED_GREEN, green);
    digitalWrite(PIN_LED_YELLOW, yellow);
    digitalWrite(PIN_LED_RED, red);

    /*
     * Each mirror pin simply repeats the level of its input pin: LOW while
     * the switch is ON, HIGH once water has raised the float. The buffer
     * between here and the C6 supplies the inversion, so this code must not
     * invert as well -- doing it in both places cancels out, which is what
     * the bench showed on 2026-08-22. FAULT is deliberately not
     * special-cased; the C6 gets the raw combination and can flag it itself.
     */
    digitalWrite(PIN_OUT_MID, midOn ? LOW : HIGH);
    digitalWrite(PIN_OUT_TOP, topOn ? LOW : HIGH);
}

void setup()
{
    /*
     * A watchdog reset leaves the WDT enabled with a short timeout on this
     * part, which can turn one reset into a boot loop. Clear it first.
     * This is the only direct register access in the codebase.
     */
    MCUSR = 0;
    wdt_disable();

    pinMode(PIN_FLOAT_MID, INPUT_PULLUP);
    pinMode(PIN_FLOAT_TOP, INPUT_PULLUP);

    /*
     * The mirrors idle LOW (empty tank), so set that level before switching
     * the pins to outputs. A pin configured as an output happens to start
     * LOW already, making this redundant today -- it is spelled out anyway
     * so that the idle level is stated once, in code, rather than resting on
     * an AVR default that a polarity change would silently invalidate.
     */
    digitalWrite(PIN_OUT_MID, LOW);
    digitalWrite(PIN_OUT_TOP, LOW);
    pinMode(PIN_OUT_MID, OUTPUT);
    pinMode(PIN_OUT_TOP, OUTPUT);

    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);

    /*
     * Let the input RC filters charge before the first reading is taken.
     * delay() is fine here; it is only banned in loop().
     */
    delay(INPUT_SETTLE_MS);

    initDebouncer(midFloat, PIN_FLOAT_MID);
    initDebouncer(topFloat, PIN_FLOAT_TOP);

    lastBlinkToggle = millis();
    blinkOn = true;

    wdt_enable(WDTO_1S);
}

void loop()
{
    wdt_reset();

    updateDebouncer(midFloat);
    updateDebouncer(topFloat);
    updateBlink();

    const Level level = levelFrom(midFloat.stable, topFloat.stable);
    applyOutputs(level, midFloat.stable, topFloat.stable);
}
