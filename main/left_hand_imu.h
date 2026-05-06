#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Left-hand IMU (MPU6050 at 0x69, AD0 = HIGH) ─────────────
// Detects horizontal wrist swipes to transpose the scale.
//
// Wiring:
//   SDA → GPIO 18  (shared I2C bus with right hand)
//   SCL → GPIO 19  (shared I2C bus with right hand)
//   AD0 → 3.3V     (sets address to 0x69)
//   VCC → 3.3V
//   GND → GND
//
// Swipe right → +5 semitones
// Swipe left  → −5 semitones
// Clamped to ±24 semitones (±2 octaves)

void  left_hand_imu_init(void);
void  left_hand_imu_process(void);

// Returns the current transpose offset in semitones
int   left_hand_imu_get_transpose(void);

// Returns true when Note Hold (chord latch) mode is active
// Swipe UP on the left wrist to toggle
bool  left_hand_imu_get_hold_enabled(void);

#ifdef __cplusplus
}
#endif
