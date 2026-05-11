#pragma once
#include <stdint.h>

typedef enum {
    SCALE_MAJOR = 0,
    SCALE_MINOR,
    SCALE_PENTATONIC,
    SCALE_DORIAN,
    SCALE_PHRYGIAN
} scale_type_t;

extern const uint8_t SCALE_MIDI_MAJOR[5];
extern const uint8_t SCALE_MIDI_MINOR[5];
extern const uint8_t SCALE_MIDI_PENTATONIC[5];
extern const uint8_t SCALE_MIDI_DORIAN[5];
extern const uint8_t SCALE_MIDI_PHRYGIAN[5];

inline const uint8_t* tuning_get_scale(scale_type_t scale) {
    switch (scale) {
        case SCALE_MAJOR:      return SCALE_MIDI_MAJOR;
        case SCALE_MINOR:      return SCALE_MIDI_MINOR;
        case SCALE_PENTATONIC: return SCALE_MIDI_PENTATONIC;
        case SCALE_DORIAN:     return SCALE_MIDI_DORIAN;
        case SCALE_PHRYGIAN:   return SCALE_MIDI_PHRYGIAN;
        default:               return SCALE_MIDI_MAJOR;
    }
}
