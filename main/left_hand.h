#pragma once

#include <stdint.h>
#include <stdbool.h>

#define NUM_FINGERS 5

// Finger indexing (0 = Pinkie, 1 = Ring, 2 = Middle, 3 = Pointer, 4 = Thumb)
typedef enum {
    FINGER_PINKIE = 0,
    FINGER_RING,
    FINGER_MIDDLE,
    FINGER_POINTER,
    FINGER_THUMB
} finger_t;

void left_hand_init(void);
void left_hand_process(void);
bool left_hand_is_pressed(finger_t finger);
bool left_hand_just_pressed(finger_t finger);
bool left_hand_just_released(finger_t finger);
