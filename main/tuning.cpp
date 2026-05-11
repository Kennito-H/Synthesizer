#include "tuning.h"

// C4 Major: 1, 2, 3, 4, 5
const uint8_t SCALE_MIDI_MAJOR[5]      = {60, 62, 64, 65, 67};

// C4 Natural Minor: 1, 2, b3, 4, 5
const uint8_t SCALE_MIDI_MINOR[5]      = {60, 62, 63, 65, 67};

// C4 Minor Pentatonic: 1, b3, 4, 5, b7
const uint8_t SCALE_MIDI_PENTATONIC[5] = {60, 63, 65, 67, 70};

// C4 Dorian: 1, b3, 4, 5, 6 (Highlights the natural 6th)
const uint8_t SCALE_MIDI_DORIAN[5]     = {60, 63, 65, 67, 69};

// C4 Phrygian: 1, b2, b3, 4, 5 (Highlights the minor 2nd)
const uint8_t SCALE_MIDI_PHRYGIAN[5]   = {60, 61, 63, 65, 67};
