#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum {
    ARP_MODE_UP,
    ARP_MODE_DOWN,
    ARP_MODE_PING_PONG,
    ARP_MODE_RANDOM,
    ARP_MODE_UP_DOWN_REPEAT
} arp_mode_t;

typedef struct {
    bool enabled;
    arp_mode_t mode;        // Sequence direction or pattern
    int num_octaves;        // 1 to 4 multiplier
    uint32_t base_speed_ms; // Duration of each step (e.g. 120BPM)
    float speed_multiplier; // Multiplier (e.g. 0.5, 1.0, 2.0)
    float probability;      // 0.0 to 1.0 chance a note plays
    
    // Internal State
    int current_step;       
    bool ping_pong_down;    
    TickType_t last_tick;
    bool is_playing_note;
} arpeggiator_t;

void arp_init(arpeggiator_t *arp);

// Returns the frequency (in Hz) of the current step, or 0 if no notes are held
uint8_t arp_process(arpeggiator_t *arp, const uint8_t *notes, const bool *active_notes, int num_notes);
