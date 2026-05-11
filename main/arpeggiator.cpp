#include "arpeggiator.h"
#include <stdlib.h>
#include "esp_random.h"

void arp_init(arpeggiator_t *arp) {
    srand(esp_random());
    arp->enabled = false;
    arp->mode = ARP_MODE_UP;
    arp->num_octaves = 1;
    arp->base_speed_ms = 125; // 120BPM 16th notes roughly
    arp->speed_multiplier = 1.0f;
    arp->probability = 1.0f;
    arp->current_step = -1; // -1 so first UP increment lands on index 0
    arp->ping_pong_down = false;
    arp->last_tick = xTaskGetTickCount();
    arp->is_playing_note = false;
}

uint8_t arp_process(arpeggiator_t *arp, const uint8_t *notes, const bool *active_notes, int num_notes) {
    if (!arp->enabled) return 0;

    // 1. Gather all currently held notes from the base scale
    uint8_t held[32];
    int num_held = 0;
    for (int i = 0; i < num_notes; i++) {
        if (active_notes[i]) {
            held[num_held++] = notes[i];
        }
    }

    if (num_held == 0) {
        arp->current_step = -1;
        arp->is_playing_note = false;
        return 0; // nothing to play
    }

    // 2. Expand notes across the requested octaves
    uint8_t expanded[32 * 4];
    int total_notes = 0;
    for (int oct = 0; oct < arp->num_octaves; oct++) {
        for (int i = 0; i < num_held; i++) {
            // MIDI note + 12 per octave
            expanded[total_notes++] = held[i] + (oct * 12);
        }
    }
    
    uint32_t actual_speed_ms = (uint32_t)(arp->base_speed_ms / arp->speed_multiplier);
    if (actual_speed_ms < 10) actual_speed_ms = 10;
    
    TickType_t now = xTaskGetTickCount();
    if ((now - arp->last_tick) * portTICK_PERIOD_MS >= actual_speed_ms) {
        arp->last_tick = now;
        
        switch (arp->mode) {
            case ARP_MODE_UP:
                arp->current_step++;
                if (arp->current_step >= total_notes) arp->current_step = 0;
                break;
            case ARP_MODE_DOWN:
                arp->current_step--;
                if (arp->current_step < 0) arp->current_step = total_notes - 1;
                break;
            case ARP_MODE_PING_PONG:
                if (arp->ping_pong_down) {
                    arp->current_step--;
                    if (arp->current_step <= 0) {
                        arp->current_step = 0;
                        arp->ping_pong_down = false;
                    }
                } else {
                    arp->current_step++;
                    if (arp->current_step >= total_notes - 1) {
                        arp->current_step = total_notes - 1;
                        arp->ping_pong_down = true;
                    }
                }
                break;
            case ARP_MODE_RANDOM:
                arp->current_step = rand() % total_notes;
                break;
            case ARP_MODE_UP_DOWN_REPEAT:
                if (arp->ping_pong_down) {
                    arp->current_step--;
                    if (arp->current_step < 0) {
                        arp->current_step = 0;
                        arp->ping_pong_down = false;
                    }
                } else {
                    arp->current_step++;
                    if (arp->current_step >= total_notes) {
                        arp->current_step = total_notes - 1;
                        arp->ping_pong_down = true;
                    }
                }
                break;
        }
        
        // Probability check
        float rand_val = (float)rand() / (float)RAND_MAX;
        if (rand_val <= arp->probability) {
            arp->is_playing_note = true;
        } else {
            arp->is_playing_note = false;
        }
    }
    
    if (!arp->is_playing_note) {
        return 0; // Skip this step
    }
    
    if (arp->current_step >= total_notes) arp->current_step = total_notes > 0 ? total_notes - 1 : 0;
    if (arp->current_step < 0) arp->current_step = 0;
    
    return expanded[arp->current_step];
}
