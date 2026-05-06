#include "right_hand.h"
#include <stdio.h>
#include <math.h>
#include "driver/i2c.h"
#include "mpu6050.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

#define I2C_MASTER_SCL_IO 19
#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

#define COMP_ALPHA 0.98f
#define GRAV_BETA  0.95f
#define RAD_TO_DEG (180.0f / (float)M_PI)

#define CALIB_SAMPLES 200
#define CALIB_DELAY_MS 10

static const char *TAG = "right_hand";
static mpu6050_handle_t mpu6050 = NULL;

static float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
static float grav_x = 0.0f, grav_y = 0.0f, grav_z = 0.0f;
static float gyro_bx = 0.0f, gyro_by = 0.0f, gyro_bz = 0.0f;
static bool seeded = false;
static double t_prev = 0.0;

static void i2c_bus_init(void) {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void gyro_calibrate(void) {
    mpu6050_gyro_value_t gyro;
    double sx = 0.0, sy = 0.0, sz = 0.0;

    ESP_LOGI(TAG, "Calibrating gyro — keep the board still...");
    for (int i = 0; i < CALIB_SAMPLES; i++) {
        if (mpu6050_get_gyro(mpu6050, &gyro) == ESP_OK) {
            sx += gyro.gyro_x;
            sy += gyro.gyro_y;
            sz += gyro.gyro_z;
        }
        vTaskDelay(pdMS_TO_TICKS(CALIB_DELAY_MS));
    }
    gyro_bx = (float)(sx / CALIB_SAMPLES);
    gyro_by = (float)(sy / CALIB_SAMPLES);
    gyro_bz = (float)(sz / CALIB_SAMPLES);
    ESP_LOGI(TAG, "Gyro bias: x=%.3f y=%.3f z=%.3f deg/s", gyro_bx, gyro_by, gyro_bz);
}

void right_hand_init(void) {
    i2c_bus_init();
    mpu6050 = mpu6050_create(I2C_MASTER_NUM, MPU6050_I2C_ADDRESS);
    if (mpu6050 == NULL) {
        ESP_LOGE(TAG, "MPU6050 create returned NULL");
        return;
    }

    mpu6050_config(mpu6050, ACCE_FS_4G, GYRO_FS_500DPS);
    mpu6050_wake_up(mpu6050);

    gyro_calibrate();

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t_prev = ts.tv_sec + ts.tv_nsec / 1e9;
}

void right_hand_process(void) {
    if (mpu6050 == NULL) return;

    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;

    if (mpu6050_get_acce(mpu6050, &acce) != ESP_OK) return;
    if (mpu6050_get_gyro(mpu6050, &gyro) != ESP_OK) return;

    float gx = gyro.gyro_x - gyro_bx;
    float gy = gyro.gyro_y - gyro_by;
    float gz = gyro.gyro_z - gyro_bz;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;
    float dt = (float)(now - t_prev);
    t_prev = now;

    if (!seeded) {
        grav_x = acce.acce_x;
        grav_y = acce.acce_y;
        grav_z = acce.acce_z;
    } else {
        grav_x = GRAV_BETA * grav_x + (1.0f - GRAV_BETA) * acce.acce_x;
        grav_y = GRAV_BETA * grav_y + (1.0f - GRAV_BETA) * acce.acce_y;
        grav_z = GRAV_BETA * grav_z + (1.0f - GRAV_BETA) * acce.acce_z;
    }

    float accel_pitch = atan2f(grav_x, sqrtf(grav_y * grav_y + grav_z * grav_z)) * RAD_TO_DEG;
    float accel_roll  = atan2f(grav_y, grav_z) * RAD_TO_DEG;

    if (!seeded) {
        pitch = accel_pitch;
        roll  = accel_roll;
        seeded = true;
    } else {
        pitch = COMP_ALPHA * (pitch + gy * dt) + (1.0f - COMP_ALPHA) * accel_pitch;
        roll  = COMP_ALPHA * (roll  + gx * dt) + (1.0f - COMP_ALPHA) * accel_roll;
    }
    yaw += gz * dt;
}

float right_hand_get_pitch(void) { return pitch; }
float right_hand_get_roll(void)  { return roll; }
float right_hand_get_yaw(void)   { return yaw; }

// Normalize values to [0.0, 1.0] for modulation.
// Let's assume hand pitch typically ranges from -45 to +45.
float right_hand_get_normalized_y(void) {
    float y = (pitch + 45.0f) / 90.0f;
    if (y < 0.0f) y = 0.0f;
    if (y > 1.0f) y = 1.0f;
    return y;
}

// Let's assume hand roll typically ranges from -45 to +45.
float right_hand_get_normalized_x(void) {
    float x = (roll + 45.0f) / 90.0f;
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x;
}
