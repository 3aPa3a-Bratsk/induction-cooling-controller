#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t lcd_dev;      // PCF8574 (8-bit)
    i2c_master_dev_handle_t relay_dev;    // PCF8575 (16-bit)
    i2c_master_dev_handle_t button_dev;   // PCF8575 (16-bit)
    bool initialized;
} i2c_manager_t;

esp_err_t i2c_manager_init(i2c_manager_t* manager);
esp_err_t i2c_manager_deinit(i2c_manager_t* manager);

esp_err_t i2c_write_byte(i2c_master_dev_handle_t dev, uint8_t data);
esp_err_t i2c_write_bytes(i2c_master_dev_handle_t dev, const uint8_t* data, size_t len);
esp_err_t i2c_read_bytes(i2c_master_dev_handle_t dev, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif