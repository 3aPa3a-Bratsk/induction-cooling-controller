#include "buttons.h"
#include "config.h"
#include "i2c_manager.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "BUTTONS";
static i2c_manager_t* g_i2c_manager = NULL;

static uint16_t buttons_read(button_manager_t* manager) {
    if (!manager || !manager->initialized || !g_i2c_manager) return 0xFFFF;
    
    uint8_t data[2];
    esp_err_t ret = i2c_read_bytes(g_i2c_manager->button_dev, data, 2);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button read failed: %d", ret);
        return manager->current_state;
    }
    
    return data[0] | (data[1] << 8);
}

button_manager_t* buttons_init(uint8_t addr, i2c_manager_t* i2c) {
    if (!i2c || !i2c->initialized) {
        ESP_LOGE(TAG, "I2C not initialized");
        return NULL;
    }
    
    g_i2c_manager = i2c;
    
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
    
    ESP_LOGI(TAG, "Button manager initialized at 0x%02X", addr);
    return manager;
}

bool buttons_update(button_manager_t* manager) {
    if (!manager || !manager->initialized) return false;
    
    uint16_t raw_state = buttons_read(manager);
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    if (raw_state != manager->last_state) {
        manager->last_debounce_time = current_time;
    }
    
    if ((current_time - manager->last_debounce_time) > DEBOUNCE_DELAY_MS) {
        if (raw_state != manager->current_state) {
            manager->current_state = raw_state;
            manager->last_state = raw_state;
            return true;
        }
    }
    
    manager->last_state = raw_state;
    return false;
}

bool button_is_pressed(button_manager_t* manager, uint8_t pin) {
    if (!manager || !manager->initialized || pin > 15) return false;
    return !(manager->current_state & (1 << pin));
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