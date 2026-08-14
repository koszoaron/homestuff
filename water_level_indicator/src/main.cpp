/*
 * Water level indicator for a two-float water container.
 *
 * Reads two float switches, debounces them, mirrors the debounced states to
 * a downstream ESP32-C6, and shows the level on a green/yellow/red LED.
 * See CLAUDE.md for the behaviour table and the constraints this code works
 * under (Arduino API only, no ISRs, no delay() in loop()).
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
 * The only place that knows the sensors are active LOW. Everything
 * downstream reads "true = float raised / wet".
 */
static bool readFloat(uint8_t pin)
{
    return digitalRead(pin) == LOW;
}

static void initDebouncer(Debouncer &d, uint8_t pin)
{
    const bool raw = readFloat(pin);

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
    const bool raw = readFloat(d.pin);

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

static Level levelFrom(bool mid, bool top)
{
    if (top) {
        /*
         * Top raised while the middle float is not is physically impossible
         * with working sensors, so treat it as a fault rather than as
         * "full" -- it means a sensor or its wiring has failed.
         */
        return mid ? Level::HIGH_ : Level::FAULT;
    }
    return mid ? Level::MID : Level::LOW_;
}

static void applyOutputs(Level level, bool mid, bool top)
{
    bool green = false;
    bool yellow = false;
    bool red = false;

    /* Mirrors normally just follow the debounced readings. */
    bool outMid = mid;
    bool outTop = top;

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
        /*
         * Err toward "tank full" so the downstream MCU takes the safe
         * action while a sensor is misbehaving.
         */
        outMid = true;
        outTop = true;
        break;
    }

    digitalWrite(PIN_LED_GREEN, green);
    digitalWrite(PIN_LED_YELLOW, yellow);
    digitalWrite(PIN_LED_RED, red);

    digitalWrite(PIN_OUT_MID, outMid);
    digitalWrite(PIN_OUT_TOP, outTop);
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
