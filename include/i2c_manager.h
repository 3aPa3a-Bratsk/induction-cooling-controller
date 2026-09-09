#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t lcd_dev;
    i2c_master_dev_handle_t relay_dev;
    i2c_master_dev_handle_t button_dev;
} i2c_manager_t;

esp_err_t i2c_manager_init(i2c_manager_t *i2c);
esp_err_t i2c_write_byte(i2c_master_dev_handle_t dev, uint8_t data);
esp_err_t i2c_write_bytes(i2c_master_dev_handle_t dev, const uint8_t *data, size_t len);
esp_err_t i2c_read_bytes(i2c_master_dev_handle_t dev, uint8_t *data, size_t len);
void i2c_manager_deinit(i2c_manager_t *i2c);

#endif