#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool enabled;
    uint32_t timeout_sec;
    uint32_t feed_interval_ms;
    EventGroupHandle_t event_group;
    SemaphoreHandle_t mutex;
    uint32_t last_feed_time;
    uint32_t reset_count;
    bool triggered;
} watchdog_manager_t;

// Watchdog event bits
#define WATCHDOG_EVENT_FEED     (1 << 0)
#define WATCHDOG_EVENT_TRIGGER  (1 << 1)
#define WATCHDOG_EVENT_RESET    (1 << 2)

// Public Functions
watchdog_manager_t* watchdog_init(uint32_t timeout_sec, uint32_t feed_interval_ms);
void watchdog_deinit(watchdog_manager_t* wdt);

// Feed the watchdog (called from high-priority tasks)
void watchdog_feed(watchdog_manager_t* wdt);

// Check if watchdog is triggered
bool watchdog_is_triggered(watchdog_manager_t* wdt);

// Reset watchdog statistics
void watchdog_reset_stats(watchdog_manager_t* wdt);

// Get watchdog statistics
uint32_t watchdog_get_reset_count(watchdog_manager_t* wdt);

// Task function for automatic watchdog feeding
void watchdog_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // WATCHDOG_H