#include <stdio.h>
#include <math.h>
#include "driver/i2c.h"
#include "mpu6050.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

#define I2C_MASTER_SCL_IO 19      /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 18      /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */

#define SAMPLE_PERIOD_MS 20
#define COMP_ALPHA 0.98f          // gyro weight; (1 - alpha) is accel weight
#define GRAV_BETA  0.95f          // gravity LPF weight on prior estimate
#define RAD_TO_DEG (180.0f / (float)M_PI)

#define CALIB_SAMPLES 200         // ~2 s of stillness; user must hold board still on boot
#define CALIB_DELAY_MS 10

static const char *TAG = "airsynth_left";
static mpu6050_handle_t mpu6050 = NULL;

/**
 * @brief i2c master initialization
 */
static void i2c_bus_init(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C config returned error");
        return;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C install returned error");
        return;
    }
}

/**
 * @brief i2c master initialization
 */
static void i2c_sensor_mpu6050_init(void)
{
    esp_err_t ret;

    i2c_bus_init();
    mpu6050 = mpu6050_create(I2C_MASTER_NUM, MPU6050_I2C_ADDRESS);
    if (mpu6050 == NULL) {
        ESP_LOGE(TAG, "MPU6050 create returned NULL");
        return;
    }

    ret = mpu6050_config(mpu6050, ACCE_FS_4G, GYRO_FS_500DPS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 config error");
        return;
    }

    ret = mpu6050_wake_up(mpu6050);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 wake up error");
        return;
    }
}

// Average N gyro samples while the board sits still. The result is the
// resting bias in deg/s — subtract from every later read to kill drift.
static void gyro_calibrate(float *bias_x, float *bias_y, float *bias_z)
{
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
    *bias_x = (float)(sx / CALIB_SAMPLES);
    *bias_y = (float)(sy / CALIB_SAMPLES);
    *bias_z = (float)(sz / CALIB_SAMPLES);
    ESP_LOGI(TAG, "Gyro bias: x=%.3f y=%.3f z=%.3f deg/s",
             *bias_x, *bias_y, *bias_z);
}

void app_main()
{
    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;

    i2c_sensor_mpu6050_init();

    float gyro_bx = 0.0f, gyro_by = 0.0f, gyro_bz = 0.0f;
    gyro_calibrate(&gyro_bx, &gyro_by, &gyro_bz);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double t_start = ts.tv_sec + ts.tv_nsec / 1e9;
    double t_prev = t_start;

    // Values for our filter
    float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
    float grav_x = 0.0f, grav_y = 0.0f, grav_z = 0.0f;
    bool seeded = false;

    while (1) {
        if (mpu6050_get_acce(mpu6050, &acce) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get accelerometer data");
        }
        if (mpu6050_get_gyro(mpu6050, &gyro) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get gyro data");
        }

        // Subtract resting bias before any integration.
        float gx = gyro.gyro_x - gyro_bx;
        float gy = gyro.gyro_y - gyro_by;
        float gz = gyro.gyro_z - gyro_bz;

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

        // Linear acceleration: to avoid shakes and hand jitters.
        float lin_x = acce.acce_x - grav_x;
        float lin_y = acce.acce_y - grav_y;
        float lin_z = acce.acce_z - grav_z;

        // Tilt from the smoothed gravity vector.
        float accel_pitch = atan2f(grav_x,
                                   sqrtf(grav_y * grav_y + grav_z * grav_z)) * RAD_TO_DEG;
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

        double timestamp = now - t_start;
        printf("%.6f %.2f %.2f %.2f %.3f %.3f %.3f\n",
               timestamp, pitch, roll, yaw, lin_x, lin_y, lin_z);

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}