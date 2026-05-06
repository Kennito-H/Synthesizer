#pragma once
#include <stdint.h>

typedef enum {
    SCALE_MAJOR = 0,
    SCALE_MINOR,
    SCALE_PENTATONIC,
    SCALE_DORIAN,
    SCALE_PHRYGIAN
} scale_type_t;

// C4 Major: 1, 2, 3, 4, 5
static const uint8_t SCALE_MIDI_MAJOR[5]      = {60, 62, 64, 65, 67};

// C4 Natural Minor: 1, 2, b3, 4, 5
static const uint8_t SCALE_MIDI_MINOR[5]      = {60, 62, 63, 65, 67};

// C4 Minor Pentatonic: 1, b3, 4, 5, b7
static const uint8_t SCALE_MIDI_PENTATONIC[5] = {60, 63, 65, 67, 70};

// C4 Dorian: 1, b3, 4, 5, 6 (Highlights the natural 6th)
static const uint8_t SCALE_MIDI_DORIAN[5]     = {60, 63, 65, 67, 69};

// C4 Phrygian: 1, b2, b3, 4, 5 (Highlights the minor 2nd)
static const uint8_t SCALE_MIDI_PHRYGIAN[5]   = {60, 61, 63, 65, 67};

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
