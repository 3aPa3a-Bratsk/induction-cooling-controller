#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t addr;
    uint16_t last_state;
    uint16_t current_state;
    uint32_t last_debounce_time;
    bool initialized;
} button_manager_t;

button_manager_t* buttons_init(uint8_t addr, i2c_manager_t* i2c);
bool buttons_update(button_manager_t* manager);
bool button_is_pressed(button_manager_t* manager, uint8_t pin);
bool button_is_start_pressed(button_manager_t* manager);
bool button_is_stop_pressed(button_manager_t* manager);
bool button_is_reset_pressed(button_manager_t* manager);
void buttons_deinit(button_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif