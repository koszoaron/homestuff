# CLAUDE.md — Water Level Indicator (ATmega168, MCU-02)

Guidance for Claude (and humans) working in this repository. Read this before writing or modifying any code.

## Project Overview

Firmware for a barebones **ATmega168-20PU** (inventory ID **MCU-02**, DIP-28, internal RC oscillator, no crystal) that monitors two float sensors in a water container and:

1. **Mirrors** both switch states (after debouncing) on two output pins consumed by a second microcontroller. The ATmega pins repeat the input pins; the **buffer between the two chips inverts**, so the downstream MCU ends up seeing HIGH-HIGH on an empty tank and LOW-LOW on a full one. The firmware itself does no inversion here — see the warning under "Pin Map".
2. **Drives three LEDs** indicating water level:
   - **GREEN** — level below the middle sensor (both switches ON)
   - **YELLOW** — level above middle, below top (middle switch OFF, top ON)
   - **RED** — top sensor reached (both switches OFF)
   - **FAULT** (middle switch ON while the top one is OFF — physically impossible with working sensors): blink RED at 2 Hz.

**The mirror outputs are never forced, FAULT included.** They are a pure inverse of the two debounced switch inputs in all four rows of the state table. This replaced an earlier design that asserted both outputs during FAULT so the C6 would err toward "tank full"; the owner changed his mind on 2026-08-21. FAULT still exists — it is just an **LED-only** state now. Do not reintroduce output forcing, and do not remove the blink.

### Design philosophy for this repo

This is a **home hobby project, optimised for being easy to pick up months later**, not for cycle counts or code size. The owner writes register-level bare-metal C99 professionally (STM32) and explicitly does *not* want to do that here.

Therefore:

- **Use the Arduino framework.** `pinMode`, `digitalRead`, `digitalWrite`, `millis()` are the preferred API. This is deliberate, not laziness.
- **No datasheet lookups should be required to edit this code.** If a change would require opening the ATmega168 reference manual, it is the wrong change.
- **No direct register access** unless there is genuinely no Arduino API for it (the watchdog is the one sanctioned exception, via `<avr/wdt.h>`).
- **No hardware timers, no ISRs, no sleep modes.** A plain `loop()` polling on `millis()` is correct here.
- Timing precision is irrelevant. Debouncing a mechanical float switch with `millis()` and a coarse threshold is entirely adequate.

Resource budget is not a concern: 16 KB flash / 1 KB RAM against a program this small.

## Power Rail: 3.3 V

The whole board runs from the **3.3 V rail supplied by the upstream ESP32-C6**. There is no 5 V anywhere in the deployed system. Every decision below follows from that.

- **In spec.** The plain (non-V) ATmega168 is rated 2.7–5.5 V. Its 20 MHz speed grade only applies at 4.5–5.5 V; at 3.3 V the ceiling is **10 MHz**. Running the internal RC at 8 MHz is therefore comfortably legal. **Never raise the clock above 8 MHz** on this board.
- **Logic levels match the ESP32-C6 directly.** At VCC = 3.3 V the ATmega's V<sub>OH</sub> is roughly VCC − 0.3 V, well above the C6's input threshold, and it can never exceed the C6's ~3.6 V absolute maximum. **No level shifters, no dividers** on the mirror outputs. This is the entire reason for the 3.3 V choice — do not reintroduce a 5 V supply "for margin".
- **BOD stays at 2.7 v.** That is the part's own minimum supply voltage, so it trips exactly where the chip stops being trustworthy. **Never select `bod = 4.3v`** — on a 3.3 V rail the chip would sit in permanent reset and look dead.
- Reduced noise margin: V<sub>IH</sub> is 0.6 × VCC ≈ 2.0 V, V<sub>IL</sub> is 0.3 × VCC ≈ 1.0 V. This matters for the sensor inputs — see below.

### Consequences for the hardware

**Float sensor inputs.** The internal pull-ups are weak (20–50 kΩ). Combined with a long run of cable to a tank and a smaller noise margin at 3.3 V, that is an easy way to pick up interference. Fit **external 10 kΩ pull-ups to 3.3 V** at the MCU, and an **RC filter at each input pin** (10 kΩ series + 100 nF to ground gives ~1 ms). Keep the internal pull-ups enabled anyway; they cost nothing. The 50 ms software debounce stays regardless — it solves mechanical chatter, not electrical noise, and the two problems need separate fixes.

**The float switches are normally CLOSED with the float down.** Confirmed on the bench, 2026-08-21. The contact closes the pin to ground while the float rests in its downward position and opens as water lifts it:

| Pin  | Switch | Float | Meaning |
|------|--------|-------|---------|
| LOW  | ON (closed)  | down | water is **below** this sensor |
| HIGH | OFF (open)   | up   | water has **reached** this sensor |

The trap to keep in mind: a switch being **ON means the tank is empty at that height**, so LOW-LOW is an empty tank, not a full one. This is the opposite of what the firmware originally assumed (it was written for normally-open floats) and is the reason the level mapping was reversed on 2026-08-21. Two more things follow:

- A **cut sensor cable** leaves the pin pulled up, which reads identically to a raised float — the tank reports fuller than it is. That is the safe direction for this appliance, so it is intentional; do not "fix" it by flipping the sense.
- The switch sits **closed** whenever the water is below it, i.e. most of the time, so the pull-ups sink current continuously (≈330 µA per input at 3.3 V through 10 kΩ). Irrelevant on a mains-powered always-on board — noted only so it is not mistaken for a wiring fault.

**LED series resistors must be recalculated — this is not a 5 V design.** With only 3.3 V of headroom:
- Red and yellow (V<sub>f</sub> ≈ 2.0 V): 1.3 V across the resistor. 220 Ω gives ~6 mA, 470 Ω gives ~3 mA.
- **Green is the problem.** A modern InGaN green LED has V<sub>f</sub> ≈ 3.0–3.2 V, leaving almost nothing — it will be very dim or effectively dark. Either use an old-style GaP green (V<sub>f</sub> ≈ 2.1 V), or a modern **low-current (2 mA) green**, which is bright at 1–2 mA. Check the green LED on the bench at 3.3 V before soldering anything.

**ESP32-C6 side.** Do not land the two mirror outputs on the C6's strapping pins (on the C6 these are GPIO4, GPIO5, GPIO8, GPIO9 and GPIO15 — verify against the module's own pinout). The ATmega drives those lines within milliseconds of power-up, and if both chips share a rail they boot simultaneously; a mirror output holding a strapping pin at the wrong level will change the C6's boot mode. Pick ordinary GPIOs.

## Clock, Fuses and Bootloader

### Clock: internal RC at **8 MHz**

`board_build.f_cpu = 8000000L`, `board_hardware.oscillator = internal`. No crystal.

This requires clearing the CKDIV8 fuse from the factory default (which gives 1 MHz). Do it — reasons:

- The Arduino core runs Timer0 at prescaler 64, so `millis()` advances in steps of 256 × 64 / F_CPU. At 1 MHz that is **16.4 ms per tick**, which is coarse relative to a 50 ms debounce window. At 8 MHz it is 2.05 ms — comfortable.
- Leaves headroom if `Serial` debug output is ever added.
- Changing CKDIV8 does **not** touch CKSEL, so the chip cannot be locked out. This is a safe fuse change.

### Bootloader: **none**

`board_hardware.uart = no_bootloader`.

Rationale — this decision is settled, do not revisit it in code:

- The board is programmed exclusively over **ISP** (Arduino UNO running ArduinoISP, or a Bus Pirate). Both are already on hand.
- Using a bootloader would additionally require a USB-serial adapter plus a DTR auto-reset cap on the breadboard. None of that exists on this build.
- No bootloader means no boot delay, the full 16 KB of flash, and no bootloader-related watchdog interactions.
- Uploads go through `pio run -t upload` with a programmer, which overwrites the whole flash anyway.

**If a bootloader is ever wanted**, the choice is **urboot**, which is what MiniCore v3+ installs by default (it replaced Optiboot). Set `board_hardware.uart = uart0` and `upload_protocol = urclock`, then `pio run -t bootloader` over ISP. Urboot is the right pick here specifically because it has **automatic baud rate detection** — the failure mode that plagues bootloaders on crystal-less internal-RC builds is UART framing errors from oscillator drift, and auto-baud absorbs that. Requires a reasonably current `platform-atmelavr` (older ones ship an avrdude without the `urclock` programmer).

**Do not install the stock Arduino (ATmegaBOOT) bootloader.** It is large by ATmega168 standards, slow, and old bootloaders of that lineage do not clear `WDRF` / disable the watchdog on entry — with the WDT enabled (see below) that can produce a reset loop that looks like a dead board.

### Fuse settings

```ini
board_build.f_cpu = 8000000L
board_hardware.oscillator = internal
board_hardware.uart = no_bootloader
board_hardware.bod = 2.7v          ; brown-out detection ON — this is a wet-environment appliance
board_hardware.eesave = yes
```

Burn once per chip, over ISP.

### ⚠ Do NOT use `-t fuses` on this part — it silently disables BOD

**`pio run -t fuses` writes the wrong extended fuse on the ATmega168.** This is a
bug in the PlatformIO `atmelavr` platform, not a configuration mistake, and it
defeats the one fuse this project cares most about.

In `~/.platformio/platforms/atmelavr/builder/fuses.py`, `get_efuse()` sorts the
ATmega168 into `targets_3`, whose branch ignores the `bod` argument entirely:

```python
elif target in targets_3:          # atmega168, atmega88, ...
    if is_urboot_or_noboot:
        return 0xFF                # BODLEVEL=111 -> BOD DISABLED
    else:
        return 0xFD
```

`targets_1` (ATmega328P, identical BODLEVEL encoding) maps `2.7v` to `0xFD`
correctly, so this is specific to the '168/'88. With `uart = no_bootloader`
set above, the console prints a reassuring `BOD level = 2.7v` and then burns
`efuse = 0xFF`, leaving brown-out detection **off** on a wet-environment
appliance. Verified on platform `atmelavr` 5.3.0 / avrdude 8.1, 2026-08-19.

Burn the fuse bytes literally instead, and read them back:

```bash
avrdude -c stk500v1 -P /dev/ttyACM0 -b 19200 -p m168 \
        -U efuse:w:0xFD:m -U hfuse:w:0xD5:m -U lfuse:w:0xE2:m \
        -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h
```

Write `lfuse` **last** — it is the one that moves the chip onto the internal RC,
and on a chip fused for a crystal that is not present, everything before it has
to happen on whatever marginal clock the part is limping along on.

| Fuse | Value | Meaning |
|-------|--------|---------|
| lfuse | `0xE2` | internal 8 MHz RC, CKDIV8 off, CKOUT off |
| hfuse | `0xD5` | SPIEN on, RSTDISBL off, EESAVE on, BOOTRST off (no bootloader) |
| efuse | `0xFD` | BODLEVEL=101 → **BOD 2.7 V** |

MCU-02 was burned to exactly these values on 2026-08-19 and read back verified.

Never touch **RSTDISBL** or **SPIEN**. Never set the oscillator to external — there is no crystal on this board.

### Chips are not necessarily blank

MCU-02 arrived fused for an **external 8–16 MHz crystal** (`lfuse = 0xFF`), not at
the factory default of `0x62`. With no crystal on the board, XTAL1 floats and the
oscillator amplifier self-oscillates on stray noise somewhere in the tens of kHz.
The symptom is distinctive and easy to misread as bad wiring: a 3-byte signature
read succeeds at a low enough ISP clock, but any real flash write comes back with
verification mismatches.

If a chip behaves this way, feed a clock into **XTAL1 (DIP pin 9)** — an Arduino
pin toggling at ~1 MHz is fine, leave XTAL2 unconnected — then burn `lfuse` to
put it on the internal RC and remove the injected clock. Always read the fuses
before concluding a target is miswired.

## Toolchain & Build System

PlatformIO, `atmelavr` platform, MiniCore board definition, Arduino framework.

```bash
pip install platformio     # once

pio run                              # build
pio run -e uno_isp -t upload         # flash via Arduino UNO as ISP
pio run -e buspirate -t upload       # flash via Bus Pirate
pio run -t clean

# Fuses: do NOT use `-t fuses` on the ATmega168 -- it disables BOD.
# See "Do NOT use -t fuses on this part" above for the avrdude command.
```

### platformio.ini (canonical contents)

```ini
[platformio]
default_envs = uno_isp

[env]
platform = atmelavr
framework = arduino
board = ATmega168
board_build.f_cpu = 8000000L
board_build.variant = standard

board_hardware.oscillator = internal
board_hardware.uart = no_bootloader
board_hardware.bod = 2.7v
board_hardware.eesave = yes

build_flags = -Wall

; --- Arduino UNO running the stock ArduinoISP sketch ---
[env:uno_isp]
upload_protocol = stk500v1
upload_port = /dev/ttyACM0          ; adjust
board_upload.speed = 19200          ; ArduinoISP's own baud rate
; -e forces an explicit chip erase -- see "Uploads need an explicit -e" below.
upload_flags =
    -P$UPLOAD_PORT
    -b$UPLOAD_SPEED
    -e

; --- Bus Pirate as ISP ---
[env:buspirate]
upload_protocol = buspirate
upload_port = /dev/ttyUSB0          ; adjust
```

Notes:
- No `-Werror`. The Arduino core headers emit warnings; failing the build on them is pure friction.
- `board_build.variant = standard` gives UNO-compatible digital pin numbering (PD2 = D2, PB0 = D8, …).

### ⚠ Uploads need an explicit `-e`

avrdude's default auto-erase does **not** reliably fire through ArduinoISP on this
target. Without `-e` the write lands on non-erased flash, so what ends up in the
chip is the bitwise AND of the old and new contents, and verification fails:

```
avrdude: 1530 bytes of flash written
avrdude warning: verification mismatch
        device 0xf8 != input 0xfb at addr 0x00ee (error)
```

The diagnostic that identifies this rather than a flaky link: the mismatch is
**bit-for-bit reproducible at the same address** across runs, and the device byte
only ever has *extra* bits cleared relative to the input (`0xf8 = 0xfb & …`).
Marginal SPI signalling produces mismatches that move around; a missing erase
does not. Adding `-e` to `upload_flags` fixed it immediately, and the same
mismatch had appeared once before on this rig for the same reason.

Chip erase does not touch fuses, and `eesave = yes` protects the EEPROM, so `-e`
is free here — there is no bootloader to preserve either.

### ⚠ ArduinoISP needs a 10 µF cap on the **Uno's** RESET

Opening `/dev/ttyACM0` asserts DTR and resets the Uno. Its bootloader then owns
the port at 115200 while avrdude is speaking 19200, and the upload dies before it
reaches the target:

```
avrdude error: protocol expects sync byte 0x14 but got 0x01
```

Fit **10 µF from the Uno's RESET to the Uno's GND** (negative leg to GND) to
defeat the auto-reset. Two things to know:

- Those `0x14` / `0x10` bytes are the *Uno's own bootloader* replying. Coherent
  STK500 chatter at the wrong baud means the auto-reset problem; a genuinely
  broken ISP link to the target gives timeouts or `not in sync` with no reply.
  Do not go looking at the target wiring for this error.
- The cap belongs on the **programmer**, not the target. It has been fitted
  across the target ATmega168's RESET by mistake before, which looks identical
  from the host side, does not help, and additionally interferes with the
  programmer driving target RESET.

### Programming harness (chip on breadboard)

| Signal | ATmega168 DIP pin | UNO-as-ISP | Bus Pirate |
|--------|-------------------|------------|------------|
| RESET  | 1 (PC6)           | D10        | CS         |
| MOSI   | 17 (PB3)          | D11        | MOSI       |
| MISO   | 18 (PB4)          | D12        | MISO       |
| SCK    | 19 (PB5)          | D13        | CLK        |
| VCC    | 7 (+ AVCC pin 20) | 5V         | 5V rail    |
| GND    | 8, 22             | GND        | GND        |

Breadboard minimums: 100 nF decoupling on VCC **and** AVCC, 10 kΩ pull-up on RESET.

### ⚠ Programming a 3.3 V target

A stock Arduino UNO is a **5 V** board. Driving its 5 V RESET/MOSI/SCK outputs into an ATmega168 powered at 3.3 V exceeds the absolute-maximum input rating of VCC + 0.5 V. It usually appears to work — the internal clamp diodes conduct — but it injects current into the supply, can latch up, and is not something to leave in the build instructions.

In order of preference:

1. **Use the Bus Pirate.** Its logic is 3.3 V natively and it can supply the 3.3 V rail itself. For this board it is simply the right tool, and the UNO is the fallback rather than the default.
2. **Program the chip out of circuit at 5 V.** It is a DIP-28 in a socket: pull it, flash it in a programming jig running at 5 V with the UNO, put it back. Perfectly safe, since nothing else is connected at the time. Never apply 5 V to the chip while it is seated next to the ESP32-C6.
3. **UNO as ISP with series resistors.** If the UNO must talk to a 3.3 V-powered target in circuit, put ~1 kΩ in series with RESET, MOSI and SCK. MISO needs nothing: 3.3 V comfortably clears the UNO's input threshold.

Whichever route: the ISP lines (D11/D12/D13) carry no application load by design, so the programmer never fights the circuit.

## Repository Layout

```
.
├── CLAUDE.md
├── platformio.ini
├── include/
│   └── config.h        ← pin map + timing constants. THE place to edit hardware details.
└── src/
    └── main.cpp        ← setup() + loop(), everything else
```

Deliberately flat. One source file is the right size for this program; do not split it into modules, and do not add an abstraction layer over `digitalWrite`.

All pin numbers and timing constants live in `config.h` as `constexpr uint8_t` / `constexpr uint16_t`. Nothing else in the codebase contains a bare pin number.

## Pin Map (single source of truth: `include/config.h`)

| Signal     | Arduino pin | Port | DIP pin | Mode           | Notes |
|------------|-------------|------|---------|----------------|-------|
| FLOAT_MID  | D2          | PD2  | 4       | `INPUT_PULLUP` | NC switch, closed to GND while the float is down → **LOW = switch ON = water below** |
| FLOAT_TOP  | D3          | PD3  | 5       | `INPUT_PULLUP` | LOW = switch ON = water below |
| OUT_MID    | D6          | PD6  | 12      | `OUTPUT`       | Debounced copy of the FLOAT_MID **pin level**; **HIGH = water reached mid**. Inverted by the buffer downstream |
| OUT_TOP    | D7          | PD7  | 13      | `OUTPUT`       | Debounced copy of the FLOAT_TOP pin level; HIGH = water reached top |
| LED_GREEN  | D8          | PB0  | 14      | `OUTPUT`       | Active HIGH, series resistor |
| LED_YELLOW | D9          | PB1  | 15      | `OUTPUT`       | Active HIGH |
| LED_RED    | D10         | PB2  | 16      | `OUTPUT`       | Active HIGH |

Polarity, in one place each:

- **Inputs**: the NC switch pulls its pin down while the float is down. `readSwitchOn()` is the only function that knows this; it returns `true` for "switch ON" and nothing downstream thinks about pin levels again.
- **Mirror outputs** do **not** invert. Each pin repeats the level of its input pin, so the ATmega drives LOW while the switch is ON and HIGH once water raises the float. The inversion the C6 wants is supplied by the **buffer** between the two chips.
- **LEDs** are active HIGH.

⚠ **The firmware must not invert the mirrors, because the output buffer already does.** This was got wrong once (2026-08-22): `applyOutputs()` inverted *and* the buffer inverted, the two cancelled, and the C6 saw every level exactly backwards while the LEDs — which do not go through the buffer — stayed correct. That combination is the signature to recognise: **LEDs right, mirrors backwards, means a double inversion in the output path, not a sensor or level-logic problem.** If the buffer is ever swapped for a non-inverting part, put the `!` back in `applyOutputs()` in the same commit.

Reserved, do not use:
- **D11/D12/D13 (PB3/PB4/PB5)** — ISP. Keeping them clear means the programmer never fights an application load.
- **D0/D1 (PD0/PD1)** — UART, kept free for future `Serial` debugging.
- **PC6** — RESET.

Unused pins are left at their default (input, no pull-up) — this is a mains-adjacent always-on appliance, not a battery device, so floating-input leakage does not matter.

## Behaviour

### Debouncing

Classic `millis()` pattern, per input:

- Read the raw pin every pass through `loop()`.
- When the raw reading differs from the last raw reading, note `millis()` as the change time.
- When the raw reading has been stable for **`DEBOUNCE_MS` = 50 ms**, commit it to the debounced state.

50 ms is a deliberately generous value — float switches slosh and chatter. If the field behaviour is still twitchy, **raise `DEBOUNCE_MS`**; do not add `delay()` calls or invent a smarter algorithm.

Absolutely no `delay()` in `loop()`. `delay()` in `setup()` is acceptable.

`millis()` rollover (~49 days) must be handled correctly by using unsigned subtraction — `millis() - lastChange >= DEBOUNCE_MS` — never `millis() >= lastChange + DEBOUNCE_MS`.

### State machine

Inputs: debounced `midOn`, `topOn` — **true = switch ON = water is below that float**. This is the canonical table; ON/OFF is the primary column because it is the only description that does not invite a polarity mistake, with the pin levels alongside it for wiring work.

| MID   | TOP   | midOn | topOn | State | LED    | OUT_MID | OUT_TOP |
|-------|-------|-------|-------|-------|--------|---------|---------|
| ON    | ON    | 1     | 1     | LOW   | GREEN  | OFF     | OFF     |
| OFF   | ON    | 0     | 1     | MID   | YELLOW | ON      | OFF     |
| OFF   | OFF   | 0     | 0     | HIGH  | RED    | ON      | ON      |
| ON    | OFF   | 1     | 0     | FAULT | RED 2 Hz | OFF   | ON      |

The same table in volts. Inputs are ON at LOW (the switch closes to GND). The ATmega's output pins simply **repeat** their input pin, and the **buffer inverts**, so the C6 ends up seeing ON = LOW:

| MID pin | TOP pin | State | OUT_MID pin | OUT_TOP pin | C6 sees MID | C6 sees TOP |
|---------|---------|-------|-------------|-------------|-------------|-------------|
| LOW     | LOW     | LOW   | LOW         | LOW         | HIGH (OFF)  | HIGH (OFF)  |
| HIGH    | LOW     | MID   | HIGH        | LOW         | LOW (ON)    | HIGH (OFF)  |
| HIGH    | HIGH    | HIGH  | HIGH        | HIGH        | LOW (ON)    | LOW (ON)    |
| LOW     | HIGH    | FAULT | LOW         | HIGH        | HIGH (OFF)  | LOW (ON)    |

The `OUT_x pin` column being identical to the matching input column is the point: **the firmware does no inversion at all on this path.**

- Exactly one LED lit at any time (blinking counts as the RED LED toggling; the other two stay off).
- The mirror outputs are a **pure inverse of the debounced switches** in every row, FAULT included. Nothing is forced and nothing is filtered, so `OUT_x` is ON exactly when water has reached float *x*.
- **FAULT** (middle switch ON while the top one is OFF) is physically impossible with working sensors and means a sensor or its wiring has failed. It shows on the **LED only**; the outputs pass the raw combination through and the C6 can flag it itself.
- FAULT is not latched — it clears as soon as the switch combination becomes valid again.
- The 2 Hz blink is another `millis()` comparison against `BLINK_MS = 250`, not `delay()`.
- ⚠ **A dead ATmega now reads as "empty", not "full".** Its pins go low or floating, the inverting buffer turns that into HIGH-HIGH, and the C6 reads an empty tank — so a failed MCU is indistinguishable from a normal empty tank and silently stops reporting. The earlier design erred toward "full" instead. If that matters once this feeds Home Assistant, solve it on the C6 side with a heartbeat or a staleness timeout rather than by re-inverting the mirrors, which would undo the fix above.

Implement as a `switch` on an `enum class Level { LOW_, MID, HIGH_, FAULT }` (or plain `enum` — note `LOW`/`HIGH` are Arduino macros, so the enumerators need different names).

### Power-up order of the mirror pins

`setup()` writes the mirror pins **LOW** before calling `pinMode(…, OUTPUT)` on them. An AVR pin switched to output already starts LOW, and LOW is the empty-tank level, so this is redundant as the code stands. It is spelled out anyway so the idle level is stated once in code instead of resting on an AVR default — the ordering mattered under the previous polarity and would matter again if the polarity moved.

### Watchdog

Enable `wdt_enable(WDTO_1S)` at the end of `setup()`, `wdt_reset()` once per `loop()`. Because a watchdog reset leaves the WDT enabled with a short timeout on this part, `setup()` must begin with:

```cpp
MCUSR = 0;
wdt_disable();
```

This is the **only** sanctioned direct register access in the codebase. If it is ever more trouble than it is worth, dropping the watchdog entirely is an acceptable simplification — say so explicitly rather than half-disabling it.

## Coding Conventions

- Arduino C++ (`.cpp`), but written in a plain-C style: free functions, `static` file-scope state, no classes, no templates, no dynamic allocation, no `String`.
- Fixed-width types from `<stdint.h>`; `unsigned long` for `millis()` values.
- `constexpr` for all constants; no magic numbers in the body of the code.
- Short functions with names that say what they do. Comments explain *why*, not *what*.
- **Use C-style block comments (`/* … */`) everywhere.** `//` is reserved for temporary changes and TODOs, so that a line comment appearing in a diff is a signal that something is unfinished. Do not "modernise" existing block comments.
- Readability beats efficiency every time in this repo.

## Things Claude Must NOT Do

- Do not "optimise" the Arduino calls into direct port manipulation (`PORTB |= …`, `PIND & …`). Even if it is faster. Especially not without asking.
- Do not add timer ISRs, `sleep_mode()`, or a scheduler.
- Do not split the code into more files or introduce a HAL.
- Do not use `delay()` in `loop()`.
- Do not change the pin map without updating `config.h` **and** the table above, and calling the change out explicitly.
- Do not change fuses, and do not add a bootloader, without an explicit decision from the owner.
- Do not add `-Werror`.

