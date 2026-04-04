#include "oscillator.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include <math.h>

#define OSCILLATOR_GPIO       11 // Audio output pin
#define LEDC_TIMER            LEDC_TIMER_0
#define LEDC_MODE             LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL          LEDC_CHANNEL_0
#define LEDC_DUTY_RES         LEDC_TIMER_8_BIT // 8 bit PWM (0-255) for audio sample resolution
#define SAMPLE_RATE           16000            // 16kHz audio sample rate

static wave_type_t current_wave = WAVE_SQUARE;
static uint32_t current_freq = 0;
static float phase = 0.0f;
static esp_timer_handle_t synth_timer;

// High-resolution timer callback that acts as our audio sample engine
static void synth_timer_callback(void* arg) {
    if (current_freq == 0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        return;
    }

    float phase_inc = (float)current_freq / (float)SAMPLE_RATE;
    phase += phase_inc;
    if (phase >= 1.0f) {
        phase -= 1.0f;
    }

    uint32_t duty = 0;
    switch (current_wave) {
        case WAVE_SQUARE:
            duty = (phase < 0.5f) ? 255 : 0;
            break;
        case WAVE_SAW:
            duty = (uint32_t)(phase * 255.0f);
            break;
        case WAVE_SINE:
            // Generate sine wave and map (-1.0 to 1.0) to (0 to 255)
            duty = (uint32_t)((sinf(phase * 2.0f * M_PI) + 1.0f) * 127.5f);
            break;
    }

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void init_oscillator(void) {
    // 1. Configure LEDC as a very high-frequency carrier (62.5kHz)
    // This is well above human hearing, so the PWM pulses merge into continuous analog voltage!
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = 62500,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = OSCILLATOR_GPIO,
        .duty           = 0, // Start silent
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // 2. Start the 16kHz software synthesis timer
    const esp_timer_create_args_t timer_args = {
        .callback = &synth_timer_callback,
        .name = "synth_timer",
        .dispatch_method = ESP_TIMER_TASK
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &synth_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(synth_timer, 1000000 / SAMPLE_RATE));
}

void set_oscillator_wave(wave_type_t wave) {
    current_wave = wave;
}

void play_frequency(uint32_t freq_hz) {
    // Pass the target frequency to our background timer task
    current_freq = freq_hz;
}
