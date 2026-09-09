#include "buttons.h"
#include "config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_timer.h"

static const char* TAG = "BUTTONS";

static uint16_t buttons_read(button_manager_t* manager) {
    if (!manager || !manager->initialized) return 0xFFFF;
    
    // Send read command
    uint8_t cmd = 0x00;
    esp_err_t ret = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        manager->addr,
        &cmd,
        1,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button read command failed: %d", ret);
        return manager->current_state;
    }
    
    // Read 2 bytes
    uint8_t data[2];
    ret = i2c_master_read_from_device(
        I2C_MASTER_NUM,
        manager->addr,
        data,
        2,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button read failed: %d", ret);
        return manager->current_state;
    }
    
    return (data[0] | (data[1] << 8));
}

button_manager_t* buttons_init(uint8_t addr) {
    button_manager_t* manager = (button_manager_t*)malloc(sizeof(button_manager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate button manager");
        return NULL;
    }
    
    manager->addr = addr;
    manager->last_state = 0xFFFF;
    manager->current_state = 0xFFFF;
    manager->last_debounce_time = 0;
    manager->initialized = true;
    
    manager->current_state = buttons_read(manager);
    manager->last_state = manager->current_state;
    
    ESP_LOGI(TAG, "Button manager initialized at address 0x%02X", addr);
    return manager;
}

bool buttons_update(button_manager_t* manager) {
    if (!manager || !manager->initialized) return false;
    
    uint16_t raw_state = buttons_read(manager);
    uint32_t current_time = esp_timer_get_time() / 1000; // ms
    
    // Debounce
    if (raw_state != manager->last_state) {
        manager->last_debounce_time = current_time;
    }
    
    if ((current_time - manager->last_debounce_time) > DEBOUNCE_DELAY_MS) {
        if (raw_state != manager->current_state) {
            manager->current_state = raw_state;
            manager->last_state = raw_state;
            return true; // State changed
        }
    }
    
    manager->last_state = raw_state;
    return false;
}

bool button_is_pressed(button_manager_t* manager, uint8_t pin) {
    if (!manager || !manager->initialized || pin > 15) return false;
    return !(manager->current_state & (1 << pin)); // Active LOW
}

bool button_is_start_pressed(button_manager_t* manager) {
    return button_is_pressed(manager, BUTTON_START);
}

bool button_is_stop_pressed(button_manager_t* manager) {
    return button_is_pressed(manager, BUTTON_STOP);
}

bool button_is_reset_pressed(button_manager_t* manager) {
    return button_is_pressed(manager, BUTTON_RESET);
}

void buttons_deinit(button_manager_t* manager) {
    if (!manager) return;
    free(manager);
}