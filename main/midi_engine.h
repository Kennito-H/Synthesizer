#pragma once
#include <stdint.h>

/*
 * MIDI Out Wiring (3.3V Logic for ESP32)
 * 
 * Using a standard 5-pin DIN connector:
 * Pin 2 (Center) -> Ground
 * Pin 4 (Left of center) -> 3.3V via a 33-ohm resistor
 * Pin 5 (Right of center) -> ESP32 GPIO 17 (TX) via a 10-ohm resistor
 * 
 * Note: Looking at the female jack from the front, the pins are:
 * [1]  [4]  [2]  [5]  [3]
 */

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_VOICES 10

void midi_engine_init(void);
void midi_engine_note_on(int voice_index, uint8_t note);
void midi_engine_note_off(int voice_index);
void midi_engine_cc(uint8_t control, uint8_t value);
void midi_engine_program_change(uint8_t program);

#ifdef __cplusplus
}
#endif
