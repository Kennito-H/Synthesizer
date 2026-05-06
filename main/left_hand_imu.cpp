#include "left_hand_imu.h"
#include "mpu6050.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <time.h>

static const char *TAG = "left_hand_imu";

// AD0 = HIGH → I2C address 0x69
#define LEFT_IMU_ADDR  0x69
#define I2C_PORT       I2C_NUM_0   // Shared bus with right hand

// Swipe detection — yaw rate threshold (Z) for transpose, pitch rate (Y) for hold
#define SWIPE_THRESHOLD_DPS  80.0f   // left/right transpose
#define HOLD_THRESHOLD_DPS   200.0f  // upward flick to toggle hold (requires harder motion)
#define AXIS_DOMINANCE       1.5f    // primary axis must be this much larger than cross axis
// Minimum ms between two swipes (debounce)
#define SWIPE_DEBOUNCE_MS    500

// Transpose step per swipe (semitones) and clamp range
#define TRANSPOSE_STEP       5
#define TRANSPOSE_MAX        24   // +2 octaves
#define TRANSPOSE_MIN       -24   // -2 octaves

// Gyro calibration
#define CALIB_SAMPLES        100
#define CALIB_DELAY_MS       10

static mpu6050_handle_t imu = NULL;
static float  gyro_bias_y   = 0.0f;
static float  gyro_bias_z   = 0.0f;
static int    transpose_st  = 0;   // current offset in semitones
static bool   hold_enabled  = false; // note hold / chord latch mode
static bool   swipe_active_z = false; // left/right swipe arm
static bool   swipe_active_y = false; // up/down swipe arm
static uint32_t last_swipe_z_ms = 0;
static uint32_t last_swipe_y_ms = 0;

// ── Calibration ──────────────────────────────────────────────
static void calibrate(void) {
    mpu6050_gyro_value_t g;
    double sumY = 0.0, sumZ = 0.0;
    ESP_LOGI(TAG, "Calibrating left IMU gyro — keep still...");
    for (int i = 0; i < CALIB_SAMPLES; i++) {
        if (mpu6050_get_gyro(imu, &g) == ESP_OK) { sumY += g.gyro_y; sumZ += g.gyro_z; }
        vTaskDelay(pdMS_TO_TICKS(CALIB_DELAY_MS));
    }
    gyro_bias_y = (float)(sumY / CALIB_SAMPLES);
    gyro_bias_z = (float)(sumZ / CALIB_SAMPLES);
    ESP_LOGI(TAG, "Left IMU bias Y: %.3f  Z: %.3f deg/s", gyro_bias_y, gyro_bias_z);
}

// ── Public API ────────────────────────────────────────────────
void left_hand_imu_init(void) {
    // I2C bus is already initialised by right_hand_init() — just create handle
    imu = mpu6050_create(I2C_PORT, LEFT_IMU_ADDR);
    if (imu == NULL) {
        ESP_LOGE(TAG, "Left IMU not found — check AD0 pin and wiring");
        return;
    }
    mpu6050_config(imu, ACCE_FS_4G, GYRO_FS_500DPS);
    mpu6050_wake_up(imu);
    calibrate();
    ESP_LOGI(TAG, "Left IMU ready (addr=0x%02X)", LEFT_IMU_ADDR);
}

void left_hand_imu_process(void) {
    if (imu == NULL) return;

    mpu6050_gyro_value_t g;
    if (mpu6050_get_gyro(imu, &g) != ESP_OK) return;

    float gy = g.gyro_y - gyro_bias_y;
    float gz = g.gyro_z - gyro_bias_z;

    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

    // ── Left/Right swipe (Z axis) → transpose ─────────────────
    if (!swipe_active_z && fabsf(gz) > SWIPE_THRESHOLD_DPS && fabsf(gz) > fabsf(gy) * AXIS_DOMINANCE) {
        if ((now - last_swipe_z_ms) > SWIPE_DEBOUNCE_MS) {
            swipe_active_z  = true;
            last_swipe_z_ms = now;
            if (gz > 0.0f) {
                transpose_st += TRANSPOSE_STEP;
                if (transpose_st > TRANSPOSE_MAX) transpose_st = TRANSPOSE_MAX;
                ESP_LOGI(TAG, "Swipe RIGHT → transpose %+d st", transpose_st);
            } else {
                transpose_st -= TRANSPOSE_STEP;
                if (transpose_st < TRANSPOSE_MIN) transpose_st = TRANSPOSE_MIN;
                ESP_LOGI(TAG, "Swipe LEFT  → transpose %+d st", transpose_st);
            }
        }
    } else if (swipe_active_z && fabsf(gz) < (SWIPE_THRESHOLD_DPS * 0.4f)
               && (now - last_swipe_z_ms) > SWIPE_DEBOUNCE_MS) {
        swipe_active_z = false; // re-arm only after wrist settles AND debounce expires
    }

    // ── Upward swipe (Y axis) → toggle note hold ──────────────
    // Requires hard flick (HOLD_THRESHOLD_DPS) and Y must dominate Z so L/R swipes don't fire this
    if (!swipe_active_y && gy > HOLD_THRESHOLD_DPS && gy > fabsf(gz) * AXIS_DOMINANCE) {
        if ((now - last_swipe_y_ms) > SWIPE_DEBOUNCE_MS) {
            swipe_active_y  = true;
            last_swipe_y_ms = now;
            hold_enabled    = !hold_enabled;
            ESP_LOGI(TAG, "Swipe UP → Note Hold %s", hold_enabled ? "ON" : "OFF");
        }
    } else if (swipe_active_y && gy < (SWIPE_THRESHOLD_DPS * 0.4f)
               && (now - last_swipe_y_ms) > SWIPE_DEBOUNCE_MS) {
        swipe_active_y = false; // re-arm only after wrist settles AND debounce expires
    }
}

int left_hand_imu_get_transpose(void) {
    return transpose_st;
}

bool left_hand_imu_get_hold_enabled(void) {
    return hold_enabled;
}
