#pragma once

#include <stdint.h>
#include <stdbool.h>

void right_hand_init(void);

// Read MPU6050 and update internal state. Should be called periodically.
void right_hand_process(void);

// Get smoothed pitch (-90 to +90 degrees) mapped roughly to vertical movement
float right_hand_get_pitch(void);

// Get smoothed roll (-180 to +180 degrees) mapped roughly to twisting/horizontal
float right_hand_get_roll(void);

// Get yaw (can drift, relative rotation)
float right_hand_get_yaw(void);

// Helper functions to get normalized values (0.0 to 1.0) based on typical human hand movement ranges
// e.g., Pitch range from -45 (down) to +45 (up) maps to 0.0 to 1.0
float right_hand_get_normalized_x(void); // usually mapped to Roll
float right_hand_get_normalized_y(void); // usually mapped to Pitch
