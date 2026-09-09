#ifndef RELAY_H
#define RELAY_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t addr;
    uint16_t state;
    bool initialized;
} relay_manager_t;

relay_manager_t* relay_init(uint8_t addr, i2c_manager_t* i2c);
void relay_set(relay_manager_t* manager, uint8_t relay_num, bool state);
bool relay_get(relay_manager_t* manager, uint8_t relay_num);
void relay_set_all(relay_manager_t* manager, uint16_t state);
uint16_t relay_get_all(relay_manager_t* manager);
void relay_deinit(relay_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif