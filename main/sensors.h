#pragma once
#include <stdint.h>
#include <stdbool.h>

#define NUM_SENSORS 5
#define TOUCH_THRESHOLD 1000

typedef enum {
    SCALE_MAJOR,
    SCALE_MINOR,
    SCALE_PENTATONIC,
    SCALE_DORIAN,
    SCALE_PHRYGIAN
} scale_type_t;

const uint32_t* get_scale_frequencies(scale_type_t scale);

void init_sensors(void);
void read_sensors(bool active_sensors[NUM_SENSORS], int *num_active, int *max_val, int *hardest_pressed_sensor);

