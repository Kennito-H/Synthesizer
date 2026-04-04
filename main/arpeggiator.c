#include "arpeggiator.h"
#include <stdlib.h> // for rand()

void arp_init(arpeggiator_t *arp) {
    arp->mode = ARP_MODE_UP;
    arp->num_octaves = 1;
    arp->speed_ms = 100;
    arp->current_step = 0;
    arp->ping_pong_down = false;
    arp->last_tick = xTaskGetTickCount();
}

uint32_t arp_process(arpeggiator_t *arp, const uint32_t *base_freqs, bool *active_sensors, int num_sensors) {
    uint32_t active[10]; // max base notes
    int n_active = 0;
    
    // Gather currently active sensor frequencies
    for (int i=0; i<num_sensors; i++) {
        if (active_sensors[i]) {
            active[n_active++] = base_freqs[i];
        }
    }
    
    // Output nothing if no sensors pressed
    if (n_active == 0) return 0;
    
    // If only 1 note and 1 octave, output a continuous tone
    if (n_active == 1 && arp->num_octaves == 1) {
        return active[0];
    }
    
    int total_notes = n_active * arp->num_octaves;
    
    // Time to advance the sequence?
    TickType_t now = xTaskGetTickCount();
    if ((now - arp->last_tick) * portTICK_PERIOD_MS >= arp->speed_ms) {
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
        }
    }
    
    // Edge case constraints (in case active drops or octaves change)
    if (arp->current_step >= total_notes) arp->current_step = total_notes > 0 ? total_notes - 1 : 0;
    if (arp->current_step < 0) arp->current_step = 0;
    
    // Map chronological step index back to an ascending note multiplier
    int note_index = arp->current_step % n_active;
    int octave_shift = arp->current_step / n_active;
    
    // Multiplying frequency by 2 produces an octave jump
    uint32_t freq = active[note_index] * (1 << octave_shift);
    return freq;
}
