#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include "left_hand.h"
#include "left_hand_imu.h"
#include "right_hand.h"
#include "menu_system.h"
#include "midi_engine.h"
#include "arpeggiator.h"

static const char *TAG = "synthairsizer_main";

static void i2c_scan(void) {
    ESP_LOGI(TAG, "--- I2C scan (bus 0) ---");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Found device at 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) ESP_LOGW(TAG, "  No I2C devices found — check wiring/pullups");
    ESP_LOGI(TAG, "--- scan done (%d found) ---", found);
}

arpeggiator_t global_arp;


// Control Task: Reads sensors, handles menu, at 100Hz
static void control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Control Task Started");
    while (1) {
        left_hand_process();
        left_hand_imu_process();
        right_hand_process();
        
        menu_system_process();

        vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing Synthairsizer...");
    
    left_hand_init();
    right_hand_init();      // Initialises the I2C bus
    left_hand_imu_init();   // Second MPU6050 on same bus at 0x69
    i2c_scan();             // Log which I2C addresses respond
    midi_engine_init();
    menu_system_init();
    arp_init(&global_arp);

    ESP_LOGI(TAG, "Initialization complete. Starting FreeRTOS tasks...");

    // Control task: Core 0, standard priority.
    xTaskCreatePinnedToCore(control_task, "control_task", 8192, NULL, 5, NULL, 0);

    // app_main must not return; delete its implicit task.
    vTaskDelete(NULL);
}
