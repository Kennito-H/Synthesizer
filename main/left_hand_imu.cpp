#include "left_hand_imu.h"
#include "left_hand.h"
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

#define SWIPE_THRESHOLD_DPS  80.0f   // left/right transpose
#define HOLD_THRESHOLD_DPS   120.0f  // upward wrist extension to toggle hold
#define AXIS_DOMINANCE       1.5f    // gz must be 1.5x gx for transpose
#define HOLD_AXIS_DOMINANCE  3.0f    // gx must be 3x gz for hold (anatomical coupling guard)
// Minimum ms between two gestures (debounce)
#define SWIPE_DEBOUNCE_MS    800

// Transpose step per swipe (semitones) and clamp range
#define TRANSPOSE_STEP       5
#define TRANSPOSE_MAX        24   // +2 octaves
#define TRANSPOSE_MIN       -24   // -2 octaves

// Gyro calibration
#define CALIB_SAMPLES        100
#define CALIB_DELAY_MS       10

static mpu6050_handle_t imu = NULL;
static float  gyro_bias_x   = 0.0f;
static float  gyro_bias_z   = 0.0f;
static int    transpose_st  = 0;     // current offset in semitones
static bool   hold_enabled  = false; // note hold / chord latch mode
static bool   swipe_active_z = false; // left/right swipe arm
static bool   swipe_active_y = false; // hold flick arm
static uint32_t last_swipe_z_ms = 0;
static uint32_t last_swipe_y_ms = 0;

// ── Calibration ──────────────────────────────────────────────
static void calibrate(void) {
    mpu6050_gyro_value_t g;
    double sumX = 0.0, sumZ = 0.0;
    ESP_LOGI(TAG, "Calibrating left IMU gyro — keep still...");
    for (int i = 0; i < CALIB_SAMPLES; i++) {
        if (mpu6050_get_gyro(imu, &g) == ESP_OK) { sumX += g.gyro_x; sumZ += g.gyro_z; }
        vTaskDelay(pdMS_TO_TICKS(CALIB_DELAY_MS));
    }
    gyro_bias_x = (float)(sumX / CALIB_SAMPLES);
    gyro_bias_z = (float)(sumZ / CALIB_SAMPLES);
    ESP_LOGI(TAG, "Left IMU bias X: %.3f  Z: %.3f deg/s", gyro_bias_x, gyro_bias_z);
}

// ── Public API ────────────────────────────────────────────────
void left_hand_imu_init(void) {
    imu = mpu6050_create(I2C_PORT, LEFT_IMU_ADDR);
    if (imu == NULL) {
        ESP_LOGE(TAG, "Left IMU not found — check AD0 pin and wiring");
        return;
    }
    mpu6050_config(imu, ACCE_FS_4G, GYRO_FS_500DPS);
    mpu6050_wake_up(imu);
    vTaskDelay(pdMS_TO_TICKS(100)); 
    calibrate();
    ESP_LOGI(TAG, "Left IMU ready (addr=0x%02X)", LEFT_IMU_ADDR);
}

void left_hand_imu_process(void) {
    if (imu == NULL) return;

    mpu6050_gyro_value_t g;
    if (mpu6050_get_gyro(imu, &g) != ESP_OK) {
        static uint32_t last_err_log = 0;
        uint32_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        if (now_ms - last_err_log > 500) {
            ESP_LOGW(TAG, "mpu6050_get_gyro FAILED");
            last_err_log = now_ms;
        }
        return;
    }

    float gx = g.gyro_x - gyro_bias_x;
    float gz = g.gyro_z - gyro_bias_z;

    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

    bool any_finger_down = false;
    for (int i = 0; i < NUM_FINGERS; i++) {
        if (left_hand_is_pressed((finger_t)i)) { any_finger_down = true; break; }
    }

    // ── Left/Right swipe (Z axis) → transpose ─────────────────
    if (any_finger_down && !swipe_active_z && !swipe_active_y
        && fabsf(gz) > SWIPE_THRESHOLD_DPS && fabsf(gz) > fabsf(gx) * AXIS_DOMINANCE) {
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
        swipe_active_z  = false;
        last_swipe_z_ms = now; 
    }

    // ── Upward wrist extension (X axis, negative) → toggle note hold ──
    if (!any_finger_down && !swipe_active_y && !swipe_active_z
        && gx < -HOLD_THRESHOLD_DPS && fabsf(gx) > fabsf(gz) * HOLD_AXIS_DOMINANCE) {
        if ((now - last_swipe_y_ms) > SWIPE_DEBOUNCE_MS) {
            swipe_active_y  = true;
            last_swipe_y_ms = now;
            hold_enabled    = !hold_enabled;
            ESP_LOGI(TAG, "Wrist flick up → Note Hold %s", hold_enabled ? "ON" : "OFF");
        }
    } else if (swipe_active_y && gx > -(HOLD_THRESHOLD_DPS * 0.4f)
               && (now - last_swipe_y_ms) > SWIPE_DEBOUNCE_MS) {
        swipe_active_y  = false;
        last_swipe_y_ms = now;
    }
}

int left_hand_imu_get_transpose(void) {
    return transpose_st;
}

bool left_hand_imu_get_hold_enabled(void) {
    return hold_enabled;
}
