Проект створено на бaзi

https://github.com/LU6APR/ESP32_C3_MINI_MORSE_TRAINER

# MORSE TRAINER IAMBIC — LU6APR — Nokia 5110 v4

This version ports the verified LCD 2004 v8 logic to the separately tested Nokia 5110 / PCD8544 display.

## Preserved v8 behavior

- Iambic Keyer
- DIT GPIO2
- DAH GPIO3
- COMMAND GPIO0
- SIDETONE GPIO1
- KEY OUT GPIO6
- short COMMAND press: clear first three text rows
- long COMMAND press (>= 1 s): enter menu
- menu navigation: DIT=DOWN, DAH=UP, COMMAND=SELECT
- Speed/WPM 10..35 in 5 WPM steps
- Iambic Mode A/B
- EEPROM settings
- sidetone
- CW decoder timing
- automatic space after pause
- automatic clear after the third text row is filled

## Nokia 5110 wiring — verified by standalone test

| Nokia 5110 | ESP32-C3 Mini |
|---|---:|
| 1 VCC | 3.3V |
| 2 GND | GND |
| 3 SCE | GPIO 7 |
| 4 RST | GPIO 10 |
| 5 D/C | GPIO 8 |
| 6 DIN/MOSI | GPIO 5 |
| 7 SCLK | GPIO 4 |
| 8 LED | 3.3V |

Do not connect the display VCC to 5V.

## Display

PCD8544 84x48, U8g2 full buffer, software SPI. The font is `u8g2_font_5x7_t_cyrillic`.
The first three display rows are used for decoded CW text. Fourteen 5x7 cells fit across 84 pixels. The bottom row is used for `WPM Iambic A/B`.

## Cyrillic

The decoder contains Russian Cyrillic А-Я in addition to Latin letters, numbers and punctuation. Ё is not decoded separately because its common Morse representation is identical to Е and therefore cannot be distinguished from Е by the received Morse sequence.


## v5 additions

Menu navigation and selection now have audible feedback using the existing
sidetone output. Navigation is 800 Hz; COMMAND selection is 1200 Hz.

Russian/Latin Morse decoding is selectable in the menu:

- Language (ENG/RU)
- DIT = RUS
- DAH = ENG
- COMMAND = OK

The selected language is saved in EEPROM. This selector is necessary because
many Russian Cyrillic and Latin Morse letters have identical code sequences,
so the firmware cannot determine the intended alphabet from the Morse signal
alone.

- Manual screen clear by short COMMAND press gives a 1000 Hz confirmation beep when sidetone is enabled.


## Display layout v10

The Nokia 5110 display uses four compact text rows plus a dedicated status row.

```text
Row 1  Y=7
Row 2  Y=17
Row 3  Y=27
Row 4  Y=37
Status Y=47
```

Each text row contains 14 characters, giving 56 text positions.
The status row remains independent and is not cleared with the text buffer.
The automatic text-buffer clear therefore occurs after the fourth text row
is filled.


## Text buffer v11

The display uses four text rows of 14 characters (56 positions) plus the
independent status row.

The buffer is linear rather than circular. The 56th character remains visible.
When the 57th character is received, the old text is cleared and that 57th
character becomes the first character of the new screen. This guarantees that
the first character of a new screen is never lost.

Automatic word-space insertion never adds a leading space to a new screen.


## Start / clear workflow v12

After startup, the first text row displays centered `>READY<`.
Decoded Morse input starts on row 2 and uses rows 2-4.

When those three input rows are full, decoding pauses and the lower-right
corner displays a blinking `CLR`. The normal WPM/Iambic status is temporarily
replaced by `CLR`.

A short press of `PIN_COMMAND` confirms clearing. The screen is cleared,
`CLR` disappears, and subsequent Morse input starts on row 1 and can use all
four text rows.

A short `PIN_COMMAND` press outside the waiting state retains the existing
manual screen-clear behavior and its confirmation beep.


## Iambic A/B keyer state machine v13

The keyer state machine was replaced independently of the display, decoder,
menu, EEPROM and command logic.

Mode A:
- while both paddles are squeezed, DIT/DAH alternate;
- when both paddles are released, the current element completes and the keyer stops;
- no extra element is generated solely because a squeeze occurred earlier.

Mode B:
- while both paddles are squeezed, DIT/DAH alternate;
- when both paddles are released after a squeeze, one additional element,
  opposite to the element just transmitted, is generated;
- if the opposite paddle is still held, it is transmitted normally;
- after the additional element, the keyer stops unless another paddle request
  remains.

This follows the distinction described by Chuck Olson, WB9KZY: Mode A
completes the current element on release; Mode B sends one additional opposite
element on release. Source: https://www.morsecode.nl/modeab.pdf


## Clear screen mode v14

The settings menu contains `Clear screen` with two modes:

- `AUTO` — the text area is cleared automatically as soon as it becomes full.
- `MANUAL` — when the text area becomes full, a blinking `CLR` appears in
  the lower-right corner and decoding waits for a short `PIN_COMMAND` press.

Inside the setting:
- DIT = MANUAL
- DAH = AUTO
- COMMAND = OK

The selected mode is stored in EEPROM.

The main menu title `MENU` is centered on the first row.
A long `PIN_COMMAND` press that opens the menu produces a confirmation beep.
