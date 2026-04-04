#include "sensors.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const adc_channel_t SENSOR_CHANNELS[NUM_SENSORS] = {
    ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7
};

// C4 Major: 1, 2, 3, 4, 5
static const uint32_t SCALE_FREQ_MAJOR[NUM_SENSORS]      = {261, 293, 329, 349, 392};

// C4 Natural Minor: 1, 2, b3, 4, 5
static const uint32_t SCALE_FREQ_MINOR[NUM_SENSORS]      = {261, 293, 311, 349, 392};

// C4 Minor Pentatonic: 1, b3, 4, 5, b7
static const uint32_t SCALE_FREQ_PENTATONIC[NUM_SENSORS] = {261, 311, 349, 392, 466};

// C4 Dorian: 1, b3, 4, 5, 6 (Highlights the natural 6th)
static const uint32_t SCALE_FREQ_DORIAN[NUM_SENSORS]     = {261, 311, 349, 392, 440};

// C4 Phrygian: 1, b2, b3, 4, 5 (Highlights the minor 2nd)
static const uint32_t SCALE_FREQ_PHRYGIAN[NUM_SENSORS]   = {261, 277, 311, 349, 392};

const uint32_t* get_scale_frequencies(scale_type_t scale) {
    switch (scale) {
        case SCALE_MAJOR:      return SCALE_FREQ_MAJOR;
        case SCALE_MINOR:      return SCALE_FREQ_MINOR;
        case SCALE_PENTATONIC: return SCALE_FREQ_PENTATONIC;
        case SCALE_DORIAN:     return SCALE_FREQ_DORIAN;
        case SCALE_PHRYGIAN:   return SCALE_FREQ_PHRYGIAN;
        default:               return SCALE_FREQ_PENTATONIC;
    }
}


static adc_oneshot_unit_handle_t adc1_handle;

void init_sensors(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    for (int i = 0; i < NUM_SENSORS; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SENSOR_CHANNELS[i], &config));
    }
}

void read_sensors(bool active_sensors[NUM_SENSORS], int *num_active, int *max_val, int *hardest_pressed_sensor) {
    *num_active = 0;
    *max_val = 0;
    *hardest_pressed_sensor = -1;
    for (int i = 0; i < NUM_SENSORS; i++) {
        active_sensors[i] = false;
        int adc_val = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, SENSOR_CHANNELS[i], &adc_val));
        
        if (adc_val > TOUCH_THRESHOLD) {
            active_sensors[i] = true;
            (*num_active)++;
            
            if (adc_val > *max_val) {
                *max_val = adc_val;
                *hardest_pressed_sensor = i;
            }
        }
    }
}
