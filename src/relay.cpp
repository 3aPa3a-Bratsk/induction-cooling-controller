#include "relay.h"
#include "config.h"
#include "i2c_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char* TAG = "RELAY";
static i2c_manager_t* g_i2c_manager = NULL;
static uint16_t current_state = 0xFFFF;
static uint32_t last_update_time = 0;

//=============================================================================
// Запись в PCF8575
//=============================================================================
static esp_err_t relay_write_direct(uint16_t state) {
    if (!g_i2c_manager || !g_i2c_manager->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t data[2];
    // Пробуем прямой порядок
    data[0] = (uint8_t)(state & 0xFF);
    data[1] = (uint8_t)((state >> 8) & 0xFF);
    
    esp_err_t ret = i2c_write_bytes(g_i2c_manager->relay_dev, data, 2);
    
    // Если не получилось - пробуем обратный
    if (ret != ESP_OK) {
        data[0] = (uint8_t)((state >> 8) & 0xFF);
        data[1] = (uint8_t)(state & 0xFF);
        ret = i2c_write_bytes(g_i2c_manager->relay_dev, data, 2);
    }
    
    return ret;
}

//=============================================================================
// Инициализация
//=============================================================================
relay_manager_t* relay_init(uint8_t addr, i2c_manager_t* i2c) {
    if (!i2c || !i2c->initialized) {
        ESP_LOGE(TAG, "I2C not initialized");
        return NULL;
    }
    
    g_i2c_manager = i2c;
    
    relay_manager_t* manager = (relay_manager_t*)malloc(sizeof(relay_manager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate relay manager");
        return NULL;
    }
    
    manager->addr = addr;
    manager->state = 0xFFFF;
    manager->initialized = true;
    
    current_state = 0xFFFF;
    last_update_time = 0;
    
    // Все реле выключены
    esp_err_t ret = relay_write_direct(0xFFFF);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Relay manager initialized at 0x%02X", addr);
    } else {
        ESP_LOGE(TAG, "Relay init failed: %d", ret);
    }
    
    return manager;
}

//=============================================================================
// Установка реле
//=============================================================================
void relay_set(relay_manager_t* manager, uint8_t relay_num, bool state) {
    if (!manager || !manager->initialized || relay_num > 15) {
        return;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Проверяем задержку
    if (current_time - last_update_time < RELAY_HOLD_TIME_MS) {
        return;
    }
    
    uint16_t new_state = manager->state;
    
    if (state) {
        new_state &= ~(1 << relay_num);
    } else {
        new_state |= (1 << relay_num);
    }
    
    if (new_state != manager->state) {
        ESP_LOGI(TAG, "Relay %d: %s", relay_num, state ? "ON" : "OFF");
        
        manager->state = new_state;
        current_state = new_state;
        
        esp_err_t ret = relay_write_direct(new_state);
        if (ret == ESP_OK) {
            last_update_time = current_time;
        } else {
            ESP_LOGE(TAG, "Relay write failed: %d", ret);
        }
    }
}

bool relay_get(relay_manager_t* manager, uint8_t relay_num) {
    if (!manager || !manager->initialized || relay_num > 15) return false;
    return !(manager->state & (1 << relay_num));
}

void relay_set_all(relay_manager_t* manager, uint16_t state) {
    if (!manager || !manager->initialized) return;
    
    if (manager->state != state) {
        manager->state = state;
        current_state = state;
        relay_write_direct(state);
        last_update_time = esp_timer_get_time() / 1000;
    }
}

uint16_t relay_get_all(relay_manager_t* manager) {
    if (!manager) return 0xFFFF;
    return manager->state;
}

void relay_deinit(relay_manager_t* manager) {
    if (!manager) return;
    if (manager->initialized) {
        relay_write_direct(0xFFFF);
    }
    free(manager);
}