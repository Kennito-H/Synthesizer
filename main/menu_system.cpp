#include "menu_system.h"
#include "left_hand.h"
#include "left_hand_imu.h"
#include "right_hand.h"
#include "midi_engine.h"
#include "arpeggiator.h"
#include "tuning.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "menu";

static menu_state_t current_state    = STATE_KEYBOARD;
static mod_assign_t active_mod_assign = MOD_ASSIGN_FILTER;

static scale_type_t current_scale = SCALE_PENTATONIC;
extern arpeggiator_t global_arp; // Provided by main.cpp
static int last_arp_step = -1;

// Used to save/restore the arp enabled state when entering preview sub-menus.
static bool arp_was_enabled_before_preview = false;

// Scale preview: arp plays freely for SCALE_PREVIEW_MS after a scale is selected.
#define SCALE_PREVIEW_MS 1500
static bool         scale_preview_active   = false;
static TickType_t   scale_preview_end_tick = 0;
static bool         scale_preview_arp_saved = false;

void menu_system_init(void) {
    current_state = STATE_KEYBOARD;
}

static void handle_keyboard_mode(void) {
    const uint8_t* notes = tuning_get_scale(current_scale);

    // Read right hand for modulation
    float rx = right_hand_get_normalized_x();
    float ry = right_hand_get_normalized_y();

    switch (active_mod_assign) {
        case MOD_ASSIGN_FILTER:
            midi_engine_cc(74, (uint8_t)(rx * 127)); // Filter Frequency
            midi_engine_cc(75, (uint8_t)(ry * 127)); // Filter Resonance
            break;
        case MOD_ASSIGN_SY_WAVE:
            midi_engine_cc(21, (uint8_t)(rx * 127)); // Bits: Wave (CC 21)
            midi_engine_cc(18, (uint8_t)(ry * 127)); // Bits: Osc 2 Detune (CC 18)
            break;
        case MOD_ASSIGN_EFFECTS:
            midi_engine_cc(85, (uint8_t)(ry * 127)); // Reverb Send
            midi_engine_cc(84, (uint8_t)(rx * 127)); // Delay Send
            break;
        case MOD_ASSIGN_SY_BITS:
            midi_engine_cc(22, (uint8_t)(rx * 127)); // Bits: Sample Rate Reduction (CC 22)
            midi_engine_cc(23, (uint8_t)(ry * 127)); // Bits: Bit Reduction (CC 23)
            break;
        case MOD_ASSIGN_ARP_SPEED:
            global_arp.speed_multiplier = 0.5f + ry * 2.0f; // Up makes it faster (0.5x to 2.5x)
            global_arp.probability = rx; // Right increases probability
            break;
    }

    // ── Note Hold (chord latch) ───────────────────────────────
    bool hold = left_hand_imu_get_hold_enabled();
    static bool hold_prev = false;

    // When hold is toggled OFF: release every voice immediately
    if (hold_prev && !hold) {
        for (int i = 0; i < NUM_VOICES; i++) midi_engine_note_off(i);
        ESP_LOGI(TAG, "Hold OFF — released all notes");
    }
    hold_prev = hold;

    // Latch logic for arpeggiator
    static bool latched_active[NUM_FINGERS] = {false};

    if (hold) {
        // Toggle keys on and off by tapping them
        for (int i=0; i<NUM_FINGERS; i++) {
            if (left_hand_just_pressed((finger_t)i)) {
                latched_active[i] = !latched_active[i];
            }
        }
    } else {
        for (int i=0; i<NUM_FINGERS; i++) {
            latched_active[i] = false;
        }
    }

    // Pass keys to midi engine or arpeggiator
    bool active[NUM_FINGERS];
    bool chord_changed = false;
    int xpose = left_hand_imu_get_transpose(); // semitones

    for (int i = 0; i < NUM_FINGERS; i++) {
        active[i] = left_hand_is_pressed((finger_t)i);

        if (!global_arp.enabled) {
            if (left_hand_just_pressed((finger_t)i)) {
                uint8_t note_val = notes[i] + xpose;
                if(note_val > 127) note_val = 127;
                midi_engine_note_on(i, note_val);
                chord_changed = true;
            } else if (left_hand_just_released((finger_t)i)) {
                // Since we are no longer polyphonic, hold mode only applies to arpeggiator
                midi_engine_note_off(i);
                chord_changed = true;
            }
        }
    }

    if (chord_changed && current_state == STATE_KEYBOARD) {
        ESP_LOGI(TAG, "Notes updated");
    }

    if (global_arp.enabled) {
        // Arp takes over — pass base notes, transpose is applied on output
        bool arp_active[NUM_FINGERS];
        for (int i=0; i<NUM_FINGERS; i++) {
            arp_active[i] = hold ? latched_active[i] : active[i];
        }
        uint8_t note = arp_process(&global_arp, notes, arp_active, NUM_FINGERS);
        if (note > 0) {
            if (global_arp.current_step != last_arp_step) {
                last_arp_step = global_arp.current_step;
                midi_engine_note_off(0);
                uint8_t final_note = note + xpose;
                if(final_note > 127) final_note = 127;
                midi_engine_note_on(0, final_note);
            }
        } else {
            last_arp_step = -1;
        }
    }
}

static uint32_t last_finger_press_time[NUM_FINGERS] = {0};
static int max_fingers_held = 0;

bool menu_system_process(void) {
    gesture_t gesture = left_hand_consume_gesture();

    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
    
    int current_fingers = 0;
    for (int i=0; i<NUM_FINGERS; i++) {
        if (left_hand_is_pressed((finger_t)i)) current_fingers++;
    }
    if (current_fingers > max_fingers_held) {
        max_fingers_held = current_fingers;
    }

    bool pure_single_tap[NUM_FINGERS] = {false};
    for (int i=0; i<NUM_FINGERS; i++) {
        if (left_hand_just_released((finger_t)i)) {
            if (max_fingers_held == 1) {
                pure_single_tap[i] = true;
            }
        }
    }

    if (current_fingers == 0) {
        max_fingers_held = 0;
    }

    bool any_double_tap = false;
    bool is_double_tap[NUM_FINGERS] = {false};
    for (int i=0; i<NUM_FINGERS; i++) {
        if (pure_single_tap[i]) {
            if (now - last_finger_press_time[i] < 500) {
                any_double_tap = true;
                is_double_tap[i] = true;
                last_finger_press_time[i] = 0; // Reset after detecting
            } else {
                last_finger_press_time[i] = now;
            }
        }
    }

    // GESTURE_ALL_DOUBLE_TAP: toggle between keyboard and main menu.
    // GESTURE_ALL_TAP (single): go back one level from any sub-menu.
    if (gesture == GESTURE_ALL_DOUBLE_TAP) {
        if (current_state == STATE_KEYBOARD) {
            ESP_LOGI(TAG, "Entering Main Menu");
            current_state = STATE_MAIN_MENU;
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        } else {
            // Force-quit menu entirely from any depth, restoring arp state.
            if (current_state == STATE_SUBMENU_ENV || current_state == STATE_SUBMENU_ARP_ROOT || current_state == STATE_SUBMENU_ARP_MODE || current_state == STATE_SUBMENU_ARP_OCTAVE || current_state == STATE_SUBMENU_ARP_SPEED) {
                global_arp.enabled = arp_was_enabled_before_preview;
            }
            if (scale_preview_active) {
                scale_preview_active = false;
                global_arp.enabled = scale_preview_arp_saved;
            }
            ESP_LOGI(TAG, "Exiting Menu -> Keyboard");
            current_state = STATE_KEYBOARD;
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        }
    } else if (gesture == GESTURE_ALL_TAP) {
        if (current_state == STATE_MAIN_MENU) {
            ESP_LOGI(TAG, "Exiting Menu -> Keyboard");
            current_state = STATE_KEYBOARD;
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        } else if (current_state != STATE_KEYBOARD) {
            // Restore arp state when leaving a preview sub-menu.
            if (current_state == STATE_SUBMENU_ENV || current_state == STATE_SUBMENU_ARP_ROOT || current_state == STATE_SUBMENU_ARP_MODE || current_state == STATE_SUBMENU_ARP_OCTAVE || current_state == STATE_SUBMENU_ARP_SPEED) {
                global_arp.enabled = arp_was_enabled_before_preview;
            }
            if (scale_preview_active) {
                scale_preview_active = false;
                global_arp.enabled = scale_preview_arp_saved;
            }
            ESP_LOGI(TAG, "Back to Main Menu");
            current_state = STATE_MAIN_MENU;
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        }
    }

    if (current_state == STATE_KEYBOARD) {
        handle_keyboard_mode();
        return false;
    }

    // --- MENU MODES ---
    // In menu modes, fingers act as buttons. We use just_pressed to trigger actions.

    if (current_state == STATE_MAIN_MENU) {
        if (pure_single_tap[FINGER_PINKIE]) {
            current_state = STATE_SUBMENU_OSC;
            ESP_LOGI(TAG, "Entered Menu: Select Syntakt Machine (Via Pattern Change)");
        }
        else if (pure_single_tap[FINGER_RING]) {
            arp_was_enabled_before_preview = global_arp.enabled;
            global_arp.enabled = true;
            current_state = STATE_SUBMENU_ARP_ROOT;
            ESP_LOGI(TAG, "Entered Menu: Arpeggiator Root");
        }
        else if (pure_single_tap[FINGER_MIDDLE]) {
            current_state = STATE_SUBMENU_TUNING;
            ESP_LOGI(TAG, "Entered Menu: Scale Tuning");
        }
        else if (pure_single_tap[FINGER_POINTER]) {
            current_state = STATE_SUBMENU_ENV;
            arp_was_enabled_before_preview = global_arp.enabled;
            global_arp.enabled = true;
            ESP_LOGI(TAG, "Entered Menu: Envelope Shaping (Move right hand)");
        }
        else if (pure_single_tap[FINGER_THUMB]) {
            current_state = STATE_SUBMENU_MOD;
            ESP_LOGI(TAG, "Entered Menu: Right Hand Modulation Target");
        }
    }
    else if (current_state == STATE_SUBMENU_OSC) {
        if (pure_single_tap[FINGER_PINKIE])   { ESP_LOGI(TAG, "Selected Machine: Bits (Pattern A01)");      midi_engine_program_change(0); }
        if (pure_single_tap[FINGER_RING])     { ESP_LOGI(TAG, "Selected Machine: Swarm (Pattern A02)");     midi_engine_program_change(1); }
        if (pure_single_tap[FINGER_MIDDLE])   { ESP_LOGI(TAG, "Selected Machine: Dual VCO (Pattern A03)");  midi_engine_program_change(2); }
        if (pure_single_tap[FINGER_POINTER])  { ESP_LOGI(TAG, "Selected Machine: Chord (Pattern A04)");     midi_engine_program_change(3); }
        if (pure_single_tap[FINGER_THUMB])    { ESP_LOGI(TAG, "Selected Machine: Toy (Pattern A05)");       midi_engine_program_change(4); }

        if (any_double_tap) {
            ESP_LOGI(TAG, "Confirmed selection. Returning to Main Menu.");
            current_state = STATE_MAIN_MENU;
        }
    }
    else if (current_state == STATE_SUBMENU_ARP_ROOT) {
        if (pure_single_tap[FINGER_PINKIE])   { current_state = STATE_SUBMENU_ARP_MODE;   ESP_LOGI(TAG, "Entered Arp Submenu: Mode"); }
        if (pure_single_tap[FINGER_RING])     { current_state = STATE_SUBMENU_ARP_OCTAVE; ESP_LOGI(TAG, "Entered Arp Submenu: Octave Range"); }
        if (pure_single_tap[FINGER_MIDDLE])   { current_state = STATE_SUBMENU_ARP_SPEED;  ESP_LOGI(TAG, "Entered Arp Submenu: Speed"); }
        
        if (pure_single_tap[FINGER_THUMB]) {
            ESP_LOGI(TAG, "Double tap thumb to toggle Arp ON/OFF and save.");
        }

        bool active[NUM_FINGERS] = {true, true, true, true, true};
        const uint8_t* notes = tuning_get_scale(current_scale);
        uint8_t note = arp_process(&global_arp, notes, active, NUM_FINGERS);
        if (note > 0) { midi_engine_note_off(0); midi_engine_note_on(0, note); }

        if (any_double_tap) {
            if (is_double_tap[FINGER_THUMB]) {
                arp_was_enabled_before_preview = !arp_was_enabled_before_preview;
                ESP_LOGI(TAG, "Arp Toggled! Returning to Main Menu.");
                global_arp.enabled = arp_was_enabled_before_preview;
                current_state = STATE_MAIN_MENU;
                for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
            } else if (!is_double_tap[FINGER_PINKIE] && !is_double_tap[FINGER_RING] && !is_double_tap[FINGER_MIDDLE]) {
                ESP_LOGI(TAG, "Confirmed Arp Settings. Returning to Main Menu.");
                global_arp.enabled = arp_was_enabled_before_preview;
                current_state = STATE_MAIN_MENU;
                for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
            }
        }
    }
    else if (current_state == STATE_SUBMENU_ARP_MODE) {
        if (pure_single_tap[FINGER_PINKIE])   { global_arp.mode = ARP_MODE_UP;             ESP_LOGI(TAG, "Selected Arp Mode: Up"); }
        if (pure_single_tap[FINGER_RING])     { global_arp.mode = ARP_MODE_DOWN;           ESP_LOGI(TAG, "Selected Arp Mode: Down"); }
        if (pure_single_tap[FINGER_MIDDLE])   { global_arp.mode = ARP_MODE_PING_PONG;      ESP_LOGI(TAG, "Selected Arp Mode: Ping-Pong"); }
        if (pure_single_tap[FINGER_POINTER])  { global_arp.mode = ARP_MODE_RANDOM;         ESP_LOGI(TAG, "Selected Arp Mode: Random"); }

        bool active[NUM_FINGERS] = {true, true, true, true, true};
        const uint8_t* notes = tuning_get_scale(current_scale);
        uint8_t note = arp_process(&global_arp, notes, active, NUM_FINGERS);
        if (note > 0) { 
            if (global_arp.current_step != last_arp_step) {
                last_arp_step = global_arp.current_step;
                midi_engine_note_off(0); midi_engine_note_on(0, note); 
            }
        }

        if (any_double_tap) {
            ESP_LOGI(TAG, "Confirmed Arp Mode. Returning to Arp Root.");
            current_state = STATE_SUBMENU_ARP_ROOT;
        }
    }
    else if (current_state == STATE_SUBMENU_ARP_OCTAVE) {
        if (pure_single_tap[FINGER_PINKIE])   { global_arp.num_octaves = 1; ESP_LOGI(TAG, "Selected Arp Octaves: 1"); }
        if (pure_single_tap[FINGER_RING])     { global_arp.num_octaves = 2; ESP_LOGI(TAG, "Selected Arp Octaves: 2"); }
        if (pure_single_tap[FINGER_MIDDLE])   { global_arp.num_octaves = 3; ESP_LOGI(TAG, "Selected Arp Octaves: 3"); }
        if (pure_single_tap[FINGER_POINTER])  { global_arp.num_octaves = 4; ESP_LOGI(TAG, "Selected Arp Octaves: 4"); }

        bool active[NUM_FINGERS] = {true, true, true, true, true};
        const uint8_t* notes = tuning_get_scale(current_scale);
        uint8_t note = arp_process(&global_arp, notes, active, NUM_FINGERS);
        if (note > 0) { 
            if (global_arp.current_step != last_arp_step) {
                last_arp_step = global_arp.current_step;
                midi_engine_note_off(0); midi_engine_note_on(0, note); 
            }
        }

        if (any_double_tap) {
            ESP_LOGI(TAG, "Confirmed Arp Octaves. Returning to Arp Root.");
            current_state = STATE_SUBMENU_ARP_ROOT;
        }
    }
    else if (current_state == STATE_SUBMENU_ARP_SPEED) {
        if (pure_single_tap[FINGER_PINKIE])   { global_arp.speed_multiplier = 0.5f; ESP_LOGI(TAG, "Selected Arp Speed: 1/2x"); }
        if (pure_single_tap[FINGER_RING])     { global_arp.speed_multiplier = 1.0f; ESP_LOGI(TAG, "Selected Arp Speed: 1x"); }
        if (pure_single_tap[FINGER_MIDDLE])   { global_arp.speed_multiplier = 2.0f; ESP_LOGI(TAG, "Selected Arp Speed: 2x"); }
        if (pure_single_tap[FINGER_POINTER])  { global_arp.speed_multiplier = 4.0f; ESP_LOGI(TAG, "Selected Arp Speed: 4x"); }
        if (pure_single_tap[FINGER_THUMB])    { global_arp.speed_multiplier = 8.0f; ESP_LOGI(TAG, "Selected Arp Speed: 8x"); }

        bool active[NUM_FINGERS] = {true, true, true, true, true};
        const uint8_t* notes = tuning_get_scale(current_scale);
        uint8_t note = arp_process(&global_arp, notes, active, NUM_FINGERS);
        if (note > 0) { 
            if (global_arp.current_step != last_arp_step) {
                last_arp_step = global_arp.current_step;
                midi_engine_note_off(0); midi_engine_note_on(0, note); 
            }
        }

        if (any_double_tap) {
            ESP_LOGI(TAG, "Confirmed Arp Speed. Returning to Arp Root.");
            current_state = STATE_SUBMENU_ARP_ROOT;
        }
    }
    else if (current_state == STATE_SUBMENU_TUNING) {
        // Check if an arp-based scale preview should expire
        if (scale_preview_active && xTaskGetTickCount() > scale_preview_end_tick) {
            scale_preview_active = false;
            global_arp.enabled = scale_preview_arp_saved;
            ESP_LOGI(TAG, "Scale preview ended");
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        }

        bool scale_changed = false;
        if (pure_single_tap[FINGER_PINKIE])   { current_scale = SCALE_MAJOR;      scale_changed = true; ESP_LOGI(TAG, "Selected Scale: Major"); }
        if (pure_single_tap[FINGER_RING])     { current_scale = SCALE_MINOR;      scale_changed = true; ESP_LOGI(TAG, "Selected Scale: Minor"); }
        if (pure_single_tap[FINGER_MIDDLE])   { current_scale = SCALE_PENTATONIC; scale_changed = true; ESP_LOGI(TAG, "Selected Scale: Pentatonic"); }
        if (pure_single_tap[FINGER_POINTER])  { current_scale = SCALE_DORIAN;     scale_changed = true; ESP_LOGI(TAG, "Selected Scale: Dorian"); }
        if (pure_single_tap[FINGER_THUMB])    { current_scale = SCALE_PHRYGIAN;   scale_changed = true; ESP_LOGI(TAG, "Selected Scale: Phrygian"); }

        if (scale_changed) {
            ESP_LOGI(TAG, "Scale changed — starting arp preview");
            // Save arp state and enable it temporarily for ascending preview
            scale_preview_arp_saved = global_arp.enabled;
            scale_preview_active    = true;
            scale_preview_end_tick  = xTaskGetTickCount() + pdMS_TO_TICKS(SCALE_PREVIEW_MS);
            global_arp.enabled      = true;
            global_arp.mode         = ARP_MODE_UP;
            global_arp.current_step = 0;
        }

        // Drive arp preview while active so the user hears the ascending scale
        if (scale_preview_active) {
            bool all_held[NUM_FINGERS] = {true, true, true, true, true};
            const uint8_t* notes = tuning_get_scale(current_scale);
            uint8_t note = arp_process(&global_arp, notes, all_held, NUM_FINGERS);
            if (note > 0) {
                midi_engine_note_off(0);
                midi_engine_note_on(0, note);
            }
        }

        if (any_double_tap) {
            ESP_LOGI(TAG, "Confirmed selection. Returning to Main Menu.");
            scale_preview_active = false;
            global_arp.enabled = scale_preview_arp_saved;
            current_state = STATE_MAIN_MENU;
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        }
    }
    else if (current_state == STATE_SUBMENU_ENV) {
        // Envelope shaping live preview
        float rx = right_hand_get_normalized_x(); // Decay
        float ry = right_hand_get_normalized_y(); // Attack
        midi_engine_cc(80, (uint8_t)(rx * 127)); // Amp Decay
        midi_engine_cc(79, (uint8_t)(ry * 127)); // Amp Attack
        
        // Arp is running in background because we enabled it when entering
        bool active[NUM_FINGERS] = {true, false, true, false, true}; // Fake chord
        const uint8_t* notes = tuning_get_scale(current_scale);
        uint8_t note = arp_process(&global_arp, notes, active, NUM_FINGERS);
        if (note > 0) {
            midi_engine_note_off(0);
            midi_engine_note_on(0, note);
        }

        if (any_double_tap) {
            ESP_LOGI(TAG, "Confirmed selection. Returning to Main Menu.");
            global_arp.enabled = arp_was_enabled_before_preview;
            current_state = STATE_MAIN_MENU;
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        }
    }
    else if (current_state == STATE_SUBMENU_MOD) {
        if (pure_single_tap[FINGER_PINKIE]) {
            active_mod_assign = MOD_ASSIGN_FILTER;
            ESP_LOGI(TAG, "Right Hand Mod Target: Filter Cutoff & Resonance");
        }
        if (pure_single_tap[FINGER_RING]) {
            active_mod_assign = MOD_ASSIGN_SY_WAVE;
            ESP_LOGI(TAG, "Right Hand Mod Target: SY Bits Wave & Detune (CC21, CC18)");
        }
        if (pure_single_tap[FINGER_MIDDLE]) {
            active_mod_assign = MOD_ASSIGN_EFFECTS;
            ESP_LOGI(TAG, "Right Hand Mod Target: Delay & Reverb (CC84, CC85)");
        }
        if (pure_single_tap[FINGER_POINTER]) {
            active_mod_assign = MOD_ASSIGN_SY_BITS;
            ESP_LOGI(TAG, "Right Hand Mod Target: SY Bits SRR & Bit Reduction (CC22, CC23)");
        }
        if (pure_single_tap[FINGER_THUMB]) {
            active_mod_assign = MOD_ASSIGN_ARP_SPEED;
            ESP_LOGI(TAG, "Right Hand Mod Target: Arp Speed & Probability");
        }

        if (any_double_tap) {
            ESP_LOGI(TAG, "Confirmed selection. Returning to Main Menu.");
            current_state = STATE_MAIN_MENU;
            for(int i=0; i<NUM_VOICES; i++) midi_engine_note_off(i);
        }
    }

    return true; // We are in a menu
}

menu_state_t menu_system_get_state(void) { return current_state; }
mod_assign_t menu_system_get_mod_assign(void) { return active_mod_assign; }
