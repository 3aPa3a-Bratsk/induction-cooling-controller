#include "relay.h"
#include "config.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char* TAG = "RELAY";

static void relay_write(relay_manager_t* manager) {
    if (!manager || !manager->initialized) return;
    
    uint8_t data[2] = {
        (uint8_t)(manager->state & 0xFF),
        (uint8_t)((manager->state >> 8) & 0xFF)
    };
    
    esp_err_t ret = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        manager->addr,
        data,
        2,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Relay write failed: %d", ret);
    }
}

relay_manager_t* relay_init(uint8_t addr) {
    relay_manager_t* manager = (relay_manager_t*)malloc(sizeof(relay_manager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate relay manager");
        return NULL;
    }
    
    manager->addr = addr;
    manager->state = 0xFFFF; // All relays OFF (active LOW)
    manager->initialized = true;
    
    relay_write(manager);
    ESP_LOGI(TAG, "Relay manager initialized at address 0x%02X", addr);
    
    return manager;
}

void relay_set(relay_manager_t* manager, uint8_t relay_num, bool state) {
    if (!manager || !manager->initialized || relay_num > 15) return;
    
    // Active LOW logic
    if (state) {
        manager->state &= ~(1 << relay_num); // Turn ON (LOW)
    } else {
        manager->state |= (1 << relay_num); // Turn OFF (HIGH)
    }
    
    relay_write(manager);
    ESP_LOGD(TAG, "Relay %d set to %s", relay_num, state ? "ON" : "OFF");
}

bool relay_get(relay_manager_t* manager, uint8_t relay_num) {
    if (!manager || !manager->initialized || relay_num > 15) return false;
    return !(manager->state & (1 << relay_num)); // Active LOW
}

void relay_set_all(relay_manager_t* manager, uint16_t state) {
    if (!manager || !manager->initialized) return;
    manager->state = state;
    relay_write(manager);
}

uint16_t relay_get_all(relay_manager_t* manager) {
    if (!manager) return 0xFFFF;
    return manager->state;
}

void relay_deinit(relay_manager_t* manager) {
    if (!manager) return;
    free(manager);
}