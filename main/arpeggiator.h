#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum {
    ARP_MODE_UP,
    ARP_MODE_DOWN,
    ARP_MODE_PING_PONG,
    ARP_MODE_RANDOM
} arp_mode_t;

typedef struct {
    arp_mode_t mode;        // Sequence direction or pattern
    int num_octaves;        // 1 to 4 multiplier
    uint32_t speed_ms;      // Duration of each step
    
    // Internal State
    int current_step;       
    bool ping_pong_down;    
    TickType_t last_tick;
} arpeggiator_t;

void arp_init(arpeggiator_t *arp);
uint32_t arp_process(arpeggiator_t *arp, const uint32_t *base_freqs, bool *active_sensors, int num_sensors);
