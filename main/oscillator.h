#pragma once
#include <stdint.h>

typedef enum {
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_SINE
} wave_type_t;

void init_oscillator(void);
void set_oscillator_wave(wave_type_t wave);
void play_frequency(uint32_t freq_hz);
