#include "left_hand.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Per-finger thresholds — finger 4 (thumb) FSR reads much lower than others
static const int TOUCH_ON_THRESH[NUM_FINGERS]  = {200, 200, 200, 200, 15};
static const int TOUCH_OFF_THRESH[NUM_FINGERS] = {80,  80,  80,  80,  5};

// ADC channels for ESP32-S3: 5(GPIO6), 6(GPIO7), 7(GPIO8), 8(GPIO9), 9(GPIO10)
static const adc_channel_t SENSOR_CHANNELS[NUM_FINGERS] = {
    ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_8, ADC_CHANNEL_9
};

static adc_oneshot_unit_handle_t adc1_handle;
static bool current_state[NUM_FINGERS] = {false};
static bool previous_state[NUM_FINGERS] = {false};

void left_hand_init(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    for (int i = 0; i < NUM_FINGERS; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SENSOR_CHANNELS[i], &config));
    }
}

void left_hand_process(void) {
    for (int i = 0; i < NUM_FINGERS; i++) {
        previous_state[i] = current_state[i];

        int adc_sum = 0, adc_val = 0;
        for (int s = 0; s < 16; s++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, SENSOR_CHANNELS[i], &adc_val));
            adc_sum += adc_val;
        }
        adc_val = adc_sum / 16;
        if (!current_state[i])
            current_state[i] = (adc_val > TOUCH_ON_THRESH[i]);
        else
            current_state[i] = (adc_val > TOUCH_OFF_THRESH[i]);
    }
}

bool left_hand_is_pressed(finger_t finger) {
    return current_state[finger];
}

bool left_hand_just_pressed(finger_t finger) {
    return current_state[finger] && !previous_state[finger];
}

bool left_hand_just_released(finger_t finger) {
    return !current_state[finger] && previous_state[finger];
}

