#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATE_KEYBOARD,
    STATE_MAIN_MENU,
    STATE_SUBMENU_OSC,
    STATE_SUBMENU_ARP_ROOT,
    STATE_SUBMENU_ARP_MODE,
    STATE_SUBMENU_ARP_OCTAVE,
    STATE_SUBMENU_ARP_SPEED,
    STATE_SUBMENU_TUNING,
    STATE_SUBMENU_ENV,
    STATE_SUBMENU_MOD
} menu_state_t;

typedef enum {
    MOD_ASSIGN_FILTER = 0,
    MOD_ASSIGN_SY_WAVE,
    MOD_ASSIGN_EFFECTS,
    MOD_ASSIGN_SY_BITS,
    MOD_ASSIGN_ARP_SPEED
} mod_assign_t;

void menu_system_init(void);

// Returns true if in a menu, false if in keyboard mode
bool menu_system_process(void);

// Get the current global state (useful for routing FSRs or MPU6050)
menu_state_t menu_system_get_state(void);
mod_assign_t menu_system_get_mod_assign(void);

#ifdef __cplusplus
}
#endif
