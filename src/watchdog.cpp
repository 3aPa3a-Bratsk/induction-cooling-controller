#include "watchdog.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

static const char* TAG = "WATCHDOG";

watchdog_manager_t* watchdog_init(uint32_t timeout_sec, uint32_t feed_interval_ms) {
    watchdog_manager_t* wdt = (watchdog_manager_t*)malloc(sizeof(watchdog_manager_t));
    if (!wdt) {
        ESP_LOGE(TAG, "Failed to allocate watchdog manager");
        return NULL;
    }
    
    wdt->enabled = true;
    wdt->timeout_sec = timeout_sec;
    wdt->feed_interval_ms = feed_interval_ms;
    wdt->last_feed_time = esp_timer_get_time() / 1000;
    wdt->reset_count = 0;
    wdt->triggered = false;
    
    wdt->event_group = xEventGroupCreate();
    if (!wdt->event_group) {
        ESP_LOGE(TAG, "Failed to create watchdog event group");
        free(wdt);
        return NULL;
    }
    
    wdt->mutex = xSemaphoreCreateMutex();
    if (!wdt->mutex) {
        ESP_LOGE(TAG, "Failed to create watchdog mutex");
        vEventGroupDelete(wdt->event_group);
        free(wdt);
        return NULL;
    }
    
    // Start watchdog task
    xTaskCreatePinnedToCore(
        watchdog_task,
        "watchdog_task",
        TASK_STACK_WATCHDOG,
        wdt,
        TASK_PRIORITY_WATCHDOG,
        NULL,
        0
    );
    
    ESP_LOGI(TAG, "Watchdog initialized: timeout=%ds, feed=%dms", 
             timeout_sec, feed_interval_ms);
    return wdt;
}

void watchdog_deinit(watchdog_manager_t* wdt) {
    if (!wdt) return;
    wdt->enabled = false;
    if (wdt->event_group) vEventGroupDelete(wdt->event_group);
    if (wdt->mutex) vSemaphoreDelete(wdt->mutex);
    free(wdt);
}

void watchdog_feed(watchdog_manager_t* wdt) {
    if (!wdt || !wdt->enabled) return;
    
    xSemaphoreTake(wdt->mutex, portMAX_DELAY);
    wdt->last_feed_time = esp_timer_get_time() / 1000;
    wdt->triggered = false;
    xSemaphoreGive(wdt->mutex);
    
    xEventGroupSetBits(wdt->event_group, WATCHDOG_EVENT_FEED);
}

bool watchdog_is_triggered(watchdog_manager_t* wdt) {
    if (!wdt) return false;
    bool triggered;
    xSemaphoreTake(wdt->mutex, portMAX_DELAY);
    triggered = wdt->triggered;
    xSemaphoreGive(wdt->mutex);
    return triggered;
}

uint32_t watchdog_get_reset_count(watchdog_manager_t* wdt) {
    if (!wdt) return 0;
    uint32_t count;
    xSemaphoreTake(wdt->mutex, portMAX_DELAY);
    count = wdt->reset_count;
    xSemaphoreGive(wdt->mutex);
    return count;
}

void watchdog_reset_stats(watchdog_manager_t* wdt) {
    if (!wdt) return;
    xSemaphoreTake(wdt->mutex, portMAX_DELAY);
    wdt->reset_count = 0;
    wdt->triggered = false;
    xSemaphoreGive(wdt->mutex);
}

void watchdog_task(void *pvParameters) {
    watchdog_manager_t* wdt = (watchdog_manager_t*)pvParameters;
    ESP_LOGI(TAG, "Watchdog task started");
    
    while (wdt->enabled) {
        // Check if watchdog is fed in time
        uint32_t current_time = esp_timer_get_time() / 1000;
        uint32_t last_feed;
        
        xSemaphoreTake(wdt->mutex, portMAX_DELAY);
        last_feed = wdt->last_feed_time;
        xSemaphoreGive(wdt->mutex);
        
        if (current_time - last_feed > (wdt->timeout_sec * 1000)) {
            // Watchdog triggered!
            ESP_LOGE(TAG, "!!! WATCHDOG TRIGGERED !!!");
            ESP_LOGE(TAG, "Last feed: %d ms ago", current_time - last_feed);
            
            xSemaphoreTake(wdt->mutex, portMAX_DELAY);
            wdt->triggered = true;
            wdt->reset_count++;
            xSemaphoreGive(wdt->mutex);
            
            xEventGroupSetBits(wdt->event_group, WATCHDOG_EVENT_TRIGGER);
            
            // Trigger system reset
            ESP_LOGE(TAG, "System will restart now!");
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        }
        
        vTaskDelay(pdMS_TO_TICKS(wdt->feed_interval_ms));
    }
}