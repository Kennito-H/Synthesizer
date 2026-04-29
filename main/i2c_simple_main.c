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
#define RAD_TO_DEG (180.0f / (float)M_PI)

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

void app_main()
{
    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;

    i2c_sensor_mpu6050_init();

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double t_start = ts.tv_sec + ts.tv_nsec / 1e9;
    double t_prev = t_start;

    // Complementary Filter set up
    float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
    bool seeded = false;

    while (1) {
        if (mpu6050_get_acce(mpu6050, &acce) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get accelerometer data");
        }
        if (mpu6050_get_gyro(mpu6050, &gyro) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get gyro data");
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        double now = ts.tv_sec + ts.tv_nsec / 1e9;
        float dt = (float)(now - t_prev);
        t_prev = now;

        // Tilt from gravity vector. Stable but noisy.
        float accel_pitch = atan2f(acce.acce_x,
                                   sqrtf(acce.acce_y * acce.acce_y +
                                         acce.acce_z * acce.acce_z)) * RAD_TO_DEG;
        float accel_roll  = atan2f(acce.acce_y, acce.acce_z) * RAD_TO_DEG;

        if (!seeded) {
            pitch = accel_pitch;
            roll  = accel_roll;
            seeded = true;
        } else {
            pitch = COMP_ALPHA * (pitch + gyro.gyro_y * dt) + (1.0f - COMP_ALPHA) * accel_pitch;
            roll  = COMP_ALPHA * (roll  + gyro.gyro_x * dt) + (1.0f - COMP_ALPHA) * accel_roll;
        }
        yaw += gyro.gyro_z * dt;

        double timestamp = now - t_start;
        printf("%.6f %.2f %.2f %.2f\n", timestamp, pitch, roll, yaw);

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}