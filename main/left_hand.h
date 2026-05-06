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

typedef enum {
    GESTURE_NONE,
    GESTURE_ALL_TAP,        // Single tap of all 5 fingers
    GESTURE_ALL_DOUBLE_TAP  // Double tap of all 5 fingers
} gesture_t;

void left_hand_init(void);

// Must be called frequently (e.g., 50-100Hz) to read ADC and process gestures
void left_hand_process(void);

// Get the current instantaneous state of a finger
bool left_hand_is_pressed(finger_t finger);

// Check if a finger was JUST pressed this frame
bool left_hand_just_pressed(finger_t finger);

// Check if a finger was JUST released this frame
bool left_hand_just_released(finger_t finger);

// Consume a detected gesture (resets it to NONE so it's only handled once)
gesture_t left_hand_consume_gesture(void);

// Get the ADC value for a given finger (0 - 4095)
int left_hand_get_raw(finger_t finger);
