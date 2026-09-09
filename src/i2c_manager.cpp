#include "i2c_manager.h"
#include "config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "I2C_MANAGER";

esp_err_t i2c_manager_init(i2c_manager_t* manager) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing I2C at %d Hz", I2C_MASTER_FREQ_HZ);

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 10,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = false,
            .allow_pd = false,
        }
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &manager->bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %d", ret);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // LCD (PCF8574) - 8-bit device
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .scl_wait_us = 200,
        .flags = {
            .disable_ack_check = false,
        }
    };

    ret = i2c_master_bus_add_device(manager->bus_handle, &dev_config, &manager->lcd_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD (PCF8574) device add failed: %d", ret);
        return ret;
    }

    // Relay (PCF8575) - 16-bit device
    dev_config.device_address = RELAY_EXPANDER_ADDR;
    ret = i2c_master_bus_add_device(manager->bus_handle, &dev_config, &manager->relay_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Relay expander device add failed: %d", ret);
        return ret;
    }

    // Buttons (PCF8575) - 16-bit device
    dev_config.device_address = BUTTON_EXPANDER_ADDR;
    ret = i2c_master_bus_add_device(manager->bus_handle, &dev_config, &manager->button_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button expander device add failed: %d", ret);
        return ret;
    }

    manager->initialized = true;
    ESP_LOGI(TAG, "I2C initialized successfully");
    return ESP_OK;
}

esp_err_t i2c_manager_deinit(i2c_manager_t* manager) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }

    if (manager->lcd_dev) {
        i2c_master_bus_rm_device(manager->lcd_dev);
    }
    if (manager->relay_dev) {
        i2c_master_bus_rm_device(manager->relay_dev);
    }
    if (manager->button_dev) {
        i2c_master_bus_rm_device(manager->button_dev);
    }
    if (manager->bus_handle) {
        i2c_del_master_bus(manager->bus_handle);
    }

    manager->initialized = false;
    ESP_LOGI(TAG, "I2C deinitialized");
    return ESP_OK;
}

esp_err_t i2c_write_byte(i2c_master_dev_handle_t dev, uint8_t data) {
    return i2c_master_transmit(dev, &data, 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t i2c_write_bytes(i2c_master_dev_handle_t dev, const uint8_t* data, size_t len) {
    return i2c_master_transmit(dev, data, len, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t i2c_read_bytes(i2c_master_dev_handle_t dev, uint8_t* data, size_t len) {
    return i2c_master_receive(dev, data, len, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}