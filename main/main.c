#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sensors.h"
#include "oscillator.h"
#include "arpeggiator.h"

static const char *TAG = "sensor_oscillator";

/* --- Advanced Arpeggiator Configuration --- */
#define ARP_MODE           ARP_MODE_UP    // ARP_MODE_UP, ARP_MODE_DOWN, ARP_MODE_PING_PONG, ARP_MODE_RANDOM
#define ARP_SPEED_MS       120            // Speed of the arpeggio steps in milliseconds
#define ARP_NUM_OCTAVES    2              // Number of octaves to span (1 to 4)
#define SYNTH_WAVEFORM     WAVE_SINE      // WAVE_SQUARE, WAVE_SAW, WAVE_SINE
#define SYNTH_SCALE        SCALE_MINOR    // SCALE_MAJOR, SCALE_MINOR, SCALE_PENTATONIC, SCALE_DORIAN, SCALE_PHRYGIAN

void app_main(void) {
    ESP_LOGI(TAG, "Initializing 5-Channel Sensor Oscillator...");
    
    init_sensors();
    init_oscillator();
    set_oscillator_wave(SYNTH_WAVEFORM);

    arpeggiator_t arp;
    arp_init(&arp);
    arp.mode = ARP_MODE;
    arp.speed_ms = ARP_SPEED_MS;
    arp.num_octaves = ARP_NUM_OCTAVES;

    ESP_LOGI(TAG, "Initialization complete. Entering polling loop...");

    uint32_t current_playing_freq = 0;

    while (1) {
        bool active_sensors[NUM_SENSORS];
        int num_active;
        int max_val;
        int hardest_pressed_sensor;

        read_sensors(active_sensors, &num_active, &max_val, &hardest_pressed_sensor);

        const uint32_t* active_scale_freqs = get_scale_frequencies(SYNTH_SCALE);
        uint32_t target_freq = arp_process(&arp, active_scale_freqs, active_sensors, NUM_SENSORS);

        if (target_freq != current_playing_freq) {
            if (target_freq > 0) {
                ESP_LOGI(TAG, "Playing: %lu Hz", target_freq);
            } else {
                ESP_LOGI(TAG, "Sensors released. Stopping oscillator.");
            }
            play_frequency(target_freq);
            current_playing_freq = target_freq;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // ~100Hz poll rate for responsive arpeggio updates
    }
}
