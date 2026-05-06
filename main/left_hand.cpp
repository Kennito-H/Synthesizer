#include "left_hand.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TOUCH_ON  400  // ADC value to register a press
#define TOUCH_OFF 150  // ADC value to register a release (hysteresis prevents bounce)
#define TAP_TIME_MS 300
#define DOUBLE_TAP_GAP_MS 300

static const char *TAG = "left_hand";

// ADC channels for ESP32-S3: 5(GPIO6), 6(GPIO7), 7(GPIO8), 8(GPIO9), 9(GPIO10)
static const adc_channel_t SENSOR_CHANNELS[NUM_FINGERS] = {
    ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_8, ADC_CHANNEL_9
};


static adc_oneshot_unit_handle_t adc1_handle;

static int raw_values[NUM_FINGERS] = {0};
static bool current_state[NUM_FINGERS] = {false};
static bool previous_state[NUM_FINGERS] = {false};

// Gesture state machine
static gesture_t pending_gesture = GESTURE_NONE;
static uint32_t last_all_release_time = 0;
static uint32_t last_all_press_time = 0;
static int tap_count = 0;
static bool currently_all_pressed = false;

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
    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
    int pressed_count = 0;

    for (int i = 0; i < NUM_FINGERS; i++) {
        previous_state[i] = current_state[i];
        
        int adc_sum = 0, adc_val = 0;
        for (int s = 0; s < 8; s++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, SENSOR_CHANNELS[i], &adc_val));
            adc_sum += adc_val;
        }
        adc_val = adc_sum / 8;
        raw_values[i] = adc_val;
        if (!current_state[i])
            current_state[i] = (adc_val > TOUCH_ON);   // not pressed: need strong press to activate
        else
            current_state[i] = (adc_val > TOUCH_OFF);  // already pressed: stay on until clearly released
        
        if (current_state[i]) {
            pressed_count++;
        }
    }

    // ESP_LOGI(TAG, "Raw Sensors: [%d, %d, %d, %d, %d]", raw_values[0], raw_values[1], raw_values[2], raw_values[3], raw_values[4]);


    bool all_pressed = (pressed_count == NUM_FINGERS);

    if (all_pressed && !currently_all_pressed) {
        // Just pressed all fingers
        currently_all_pressed = true;
        last_all_press_time = now;
    } else if (!all_pressed && currently_all_pressed) {
        // Just released all fingers (or at least one)
        currently_all_pressed = false;
        uint32_t press_duration = now - last_all_press_time;
        
        if (press_duration < TAP_TIME_MS) {
            tap_count++;
            last_all_release_time = now;
        } else {
            tap_count = 0; // Was a long hold, not a tap
        }
    }

    // Evaluate taps
    if (tap_count > 0 && !currently_all_pressed) {
        if (tap_count == 1 && (now - last_all_release_time) > DOUBLE_TAP_GAP_MS) {
            // Gap expired, it's a single tap
            pending_gesture = GESTURE_ALL_TAP;
            tap_count = 0;
        } else if (tap_count == 2) {
            // It's a double tap
            pending_gesture = GESTURE_ALL_DOUBLE_TAP;
            tap_count = 0;
        }
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

gesture_t left_hand_consume_gesture(void) {
    gesture_t g = pending_gesture;
    pending_gesture = GESTURE_NONE;
    return g;
}

int left_hand_get_raw(finger_t finger) {
    return raw_values[finger];
}
