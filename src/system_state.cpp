#include "system_state.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "SYSTEM_STATE";

system_state_manager_t* system_state_init(void) {
    system_state_manager_t* manager = (system_state_manager_t*)malloc(sizeof(system_state_manager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }
    
    memset(manager, 0, sizeof(system_state_manager_t));
    
    manager->current_state = STATE_READY;
    manager->previous_state = STATE_READY;
    manager->state_change_count = 0;
    manager->runtime_seconds = 0;
    manager->alarm_count = 0;
    manager->start_count = 0;
    manager->error_count = 0;
    
    manager->last_alarm.code = ALARM_NO_ERROR;
    manager->last_alarm.channel = 0;
    strcpy(manager->last_alarm.message, "No error");
    manager->last_alarm.timestamp = 0;
    manager->last_alarm.latched = false;
    
    manager->event_group = xEventGroupCreate();
    if (!manager->event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        free(manager);
        return NULL;
    }
    
    manager->mutex = xSemaphoreCreateMutex();
    if (!manager->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        vEventGroupDelete(manager->event_group);
        free(manager);
        return NULL;
    }
    
    ESP_LOGI(TAG, "System state initialized: READY");
    return manager;
}

void system_state_deinit(system_state_manager_t* manager) {
    if (!manager) return;
    
    if (manager->event_group) {
        vEventGroupDelete(manager->event_group);
    }
    if (manager->mutex) {
        vSemaphoreDelete(manager->mutex);
    }
    free(manager);
}

void system_state_set(system_state_manager_t* manager, system_state_t state) {
    if (!manager) return;
    
    xSemaphoreTake(manager->mutex, portMAX_DELAY);
    system_state_t old_state = manager->current_state;
    manager->previous_state = old_state;
    manager->current_state = state;
    manager->state_change_count++;
    
    if (state == STATE_RUNNING) {
        manager->start_count++;
    }
    if (state == STATE_ALARM) {
        manager->alarm_count++;
    }
    
    xSemaphoreGive(manager->mutex);
    
    if (old_state != state) {
        xEventGroupSetBits(manager->event_group, EVENT_STATE_CHANGED);
        ESP_LOGI(TAG, "State changed: %s -> %s", 
                 system_state_to_string(old_state), 
                 system_state_to_string(state));
    }
}

system_state_t system_state_get(system_state_manager_t* manager) {
    if (!manager) return STATE_ERROR;
    
    system_state_t state;
    xSemaphoreTake(manager->mutex, portMAX_DELAY);
    state = manager->current_state;
    xSemaphoreGive(manager->mutex);
    return state;
}

const char* system_state_to_string(system_state_t state) {
    switch (state) {
        case STATE_READY:         return "READY";
        case STATE_RUNNING:       return "RUNNING";
        case STATE_ALARM:         return "ALARM";
        case STATE_ERROR:         return "ERROR";
        case STATE_EMERGENCY_STOP: return "EMERGENCY";
        case STATE_MAINTENANCE:   return "MAINTENANCE";
        default:                  return "UNKNOWN";
    }
}

bool system_state_is_running(system_state_manager_t* manager) {
    return system_state_get(manager) == STATE_RUNNING;
}

bool system_state_is_ready(system_state_manager_t* manager) {
    return system_state_get(manager) == STATE_READY;
}

bool system_state_is_alarm(system_state_manager_t* manager) {
    return system_state_get(manager) == STATE_ALARM;
}

bool system_state_start(system_state_manager_t* manager) {
    if (!manager) return false;
    
    if (system_state_get(manager) == STATE_READY) {
        xEventGroupSetBits(manager->event_group, EVENT_START_REQUEST);
        return true;
    }
    return false;
}

bool system_state_stop(system_state_manager_t* manager) {
    if (!manager) return false;
    
    if (system_state_get(manager) == STATE_RUNNING) {
        xEventGroupSetBits(manager->event_group, EVENT_STOP_REQUEST);
        return true;
    }
    return false;
}

bool system_state_trigger_alarm(system_state_manager_t* manager, alarm_code_t code, 
                                 uint8_t channel, const char* msg) {
    if (!manager) return false;
    
    if (system_state_get(manager) == STATE_RUNNING || 
        system_state_get(manager) == STATE_READY) {
        
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
        manager->last_alarm.code = code;
        manager->last_alarm.channel = channel;
        if (msg) {
            strncpy(manager->last_alarm.message, msg, 63);
            manager->last_alarm.message[63] = '\0';
        } else {
            strcpy(manager->last_alarm.message, "Alarm triggered");
        }
        manager->last_alarm.timestamp = esp_timer_get_time() / 1000;
        manager->last_alarm.latched = true;
        manager->alarm_count++;
        xSemaphoreGive(manager->mutex);
        
        system_state_set(manager, STATE_ALARM);
        xEventGroupSetBits(manager->event_group, EVENT_ALARM_TRIGGER);
        return true;
    }
    return false;
}

bool system_state_reset_alarm(system_state_manager_t* manager) {
    if (!manager) return false;
    
    if (system_state_get(manager) == STATE_ALARM) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
        manager->last_alarm.latched = false;
        xSemaphoreGive(manager->mutex);
        
        system_state_set(manager, STATE_READY);
        xEventGroupSetBits(manager->event_group, EVENT_RESET_REQUEST);
        return true;
    }
    return false;
}

bool system_state_emergency_stop(system_state_manager_t* manager) {
    if (!manager) return false;
    
    system_state_set(manager, STATE_EMERGENCY_STOP);
    xEventGroupSetBits(manager->event_group, EVENT_EMERGENCY_REQUEST);
    return true;
}

uint32_t system_state_get_alarm_count(system_state_manager_t* manager) {
    if (!manager) return 0;
    
    uint32_t count;
    xSemaphoreTake(manager->mutex, portMAX_DELAY);
    count = manager->alarm_count;
    xSemaphoreGive(manager->mutex);
    return count;
}

uint64_t system_state_get_runtime(system_state_manager_t* manager) {
    if (!manager) return 0;
    
    uint64_t runtime;
    xSemaphoreTake(manager->mutex, portMAX_DELAY);
    runtime = manager->runtime_seconds;
    xSemaphoreGive(manager->mutex);
    return runtime;
}

const alarm_info_t* system_state_get_last_alarm(system_state_manager_t* manager) {
    if (!manager) return NULL;
    return &manager->last_alarm;
}

void system_state_reset_statistics(system_state_manager_t* manager) {
    if (!manager) return;
    
    xSemaphoreTake(manager->mutex, portMAX_DELAY);
    manager->state_change_count = 0;
    manager->runtime_seconds = 0;
    manager->alarm_count = 0;
    manager->start_count = 0;
    manager->error_count = 0;
    xSemaphoreGive(manager->mutex);
}