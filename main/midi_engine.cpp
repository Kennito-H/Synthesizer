#include "midi_engine.h"
#include "driver/uart.h"
#include "esp_log.h"

#define MIDI_UART_NUM UART_NUM_1
#define MIDI_TX_PIN 17

#define MIDI_STATUS_NOTE_ON  (0x90 | (MIDI_CHANNEL - 1))
#define MIDI_STATUS_NOTE_OFF (0x80 | (MIDI_CHANNEL - 1))
#define MIDI_STATUS_CC       (0xB0 | (MIDI_CHANNEL - 1))
#define MIDI_STATUS_PC       (0xC0 | (MIDI_CHANNEL - 1))

static uint8_t active_notes[NUM_VOICES] = {0};
static bool    note_active[NUM_VOICES]  = {false};
static uint8_t last_cc[128];
static const char *TAG = "midi_engine";

void midi_engine_init(void) {
    for (int i = 0; i < 128; i++) last_cc[i] = 0xFF;

    uart_config_t uart_config = {
        .baud_rate = 31250,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(MIDI_UART_NUM, &uart_config);
    uart_set_pin(MIDI_UART_NUM, MIDI_TX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(MIDI_UART_NUM, 256, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "MIDI engine initialized on UART%d, TX pin %d, channel %d", MIDI_UART_NUM, MIDI_TX_PIN, MIDI_CHANNEL);
}

void midi_engine_note_on(int voice_index, uint8_t note) {
    if (voice_index < 0 || voice_index >= NUM_VOICES) return;

    if (note_active[voice_index]) {
        midi_engine_note_off(voice_index);
    }

    active_notes[voice_index] = note;
    note_active[voice_index]  = true;
    uint8_t data[3] = { MIDI_STATUS_NOTE_ON, note, NOTE_VELOCITY };
    uart_write_bytes(MIDI_UART_NUM, (const char*)data, 3);
}

void midi_engine_note_off(int voice_index) {
    if (voice_index < 0 || voice_index >= NUM_VOICES) return;

    if (note_active[voice_index]) {
        uint8_t data[3] = { MIDI_STATUS_NOTE_OFF, active_notes[voice_index], 0 };
        uart_write_bytes(MIDI_UART_NUM, (const char*)data, 3);
        note_active[voice_index] = false;
    }
}

void midi_engine_cc(uint8_t control, uint8_t value) {
    if (value == last_cc[control]) return;
    last_cc[control] = value;
    uint8_t data[3] = { MIDI_STATUS_CC, control, value };
    uart_write_bytes(MIDI_UART_NUM, (const char*)data, 3);
}

void midi_engine_program_change(uint8_t program) {
    uint8_t data[2] = { MIDI_STATUS_PC, program };
    uart_write_bytes(MIDI_UART_NUM, (const char*)data, 2);
}
