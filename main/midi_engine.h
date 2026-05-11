#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_VOICES    10
#define MIDI_CHANNEL  1    // Syntakt track 1
#define NOTE_VELOCITY 127

void midi_engine_init(void);
void midi_engine_note_on(int voice_index, uint8_t note);
void midi_engine_note_off(int voice_index);
void midi_engine_cc(uint8_t control, uint8_t value);
void midi_engine_program_change(uint8_t program);

#ifdef __cplusplus
}
#endif
