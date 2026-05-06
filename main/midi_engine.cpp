#include "midi_engine.h"
#include "driver/uart.h"
#include "esp_log.h"

#define MIDI_UART_NUM UART_NUM_1
#define MIDI_TX_PIN 17

static uint8_t active_notes[10] = {0};
static const char *TAG = "midi_engine";

void midi_engine_init(void) {
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
    ESP_LOGI(TAG, "MIDI engine initialized on UART%d, TX pin %d", MIDI_UART_NUM, MIDI_TX_PIN);
}

void midi_engine_note_on(int voice_index, uint8_t note) {
    if (voice_index < 0 || voice_index >= 10) return;
    
    // If there's already a note playing on this voice, turn it off first
    if (active_notes[voice_index] != 0) {
        midi_engine_note_off(voice_index);
    }
    
    active_notes[voice_index] = note;
    uint8_t data[3] = { 0x90, note, 127 }; // Channel 1, Velocity 127
    uart_write_bytes(MIDI_UART_NUM, (const char*)data, 3);
}

void midi_engine_note_off(int voice_index) {
    if (voice_index < 0 || voice_index >= 10) return;
    
    uint8_t note = active_notes[voice_index];
    if (note != 0) {
        uint8_t data[3] = { 0x80, note, 0 }; // Channel 1, Note Off
        uart_write_bytes(MIDI_UART_NUM, (const char*)data, 3);
        active_notes[voice_index] = 0;
    }
}

void midi_engine_cc(uint8_t control, uint8_t value) {
    uint8_t data[3] = { 0xB0, control, value }; // Channel 1, CC
    uart_write_bytes(MIDI_UART_NUM, (const char*)data, 3);
}

void midi_engine_program_change(uint8_t program) {
    uint8_t data[2] = { 0xC0, program }; // Channel 1, Program Change
    uart_write_bytes(MIDI_UART_NUM, (const char*)data, 2);
}
