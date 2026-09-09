#ifndef LCD1602_H
#define LCD1602_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t addr;
    uint8_t cols;
    uint8_t rows;
    bool initialized;
    bool backlight;
} lcd1602_t;

lcd1602_t* lcd1602_init(uint8_t addr, uint8_t cols, uint8_t rows, i2c_manager_t* i2c);
void lcd1602_print(lcd1602_t* lcd, const char* line1, const char* line2);
void lcd1602_clear(lcd1602_t* lcd);
void lcd1602_set_backlight(lcd1602_t* lcd, bool on);
void lcd1602_deinit(lcd1602_t* lcd);

#ifdef __cplusplus
}
#endif

#endif