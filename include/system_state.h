#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// State Definitions
//=============================================================================

typedef enum {
    STATE_READY = 0,        // System ready, waiting for start
    STATE_RUNNING,          // System running normally
    STATE_ALARM,            // Alarm condition
    STATE_ERROR,            // Fatal error, requires restart
    STATE_EMERGENCY_STOP,   // Emergency stop activated
    STATE_MAINTENANCE       // Maintenance mode (future)
} system_state_t;

typedef enum {
    ALARM_NO_ERROR = 0,
    ALARM_FLOW_LOSS,        // Flow lost on one or more sensors
    ALARM_FLOW_INVERTED,    // Flow detected during test mode
    ALARM_COMMUNICATION,    // I2C communication error
    ALARM_RELAY_FAILURE,    // Relay driver failure
    ALARM_BUTTON_FAILURE,   // Button expander failure
    ALARM_WATCHDOG,         // Watchdog triggered
    ALARM_EMERGENCY         // Emergency stop pressed
} alarm_code_t;

typedef struct {
    uint32_t code;
    uint8_t channel;        // Channel number for flow alarms
    char message[64];
    uint32_t timestamp;     // Time of occurrence
    bool latched;           // Alarm latched until reset
} alarm_info_t;

typedef struct {
    system_state_t current_state;
    system_state_t previous_state;
    EventGroupHandle_t event_group;
    SemaphoreHandle_t mutex;
    
    alarm_info_t last_alarm;
    uint32_t state_change_count;
    uint64_t runtime_seconds;
    uint32_t alarm_count;
    uint32_t start_count;
    uint32_t error_count;
} system_state_manager_t;

// Event bits
#define EVENT_START_REQUEST     (1 << 0)
#define EVENT_STOP_REQUEST      (1 << 1)
#define EVENT_RESET_REQUEST     (1 << 2)
#define EVENT_EMERGENCY_REQUEST (1 << 3)
#define EVENT_ALARM_TRIGGER     (1 << 4)
#define EVENT_STATE_CHANGED     (1 << 5)
#define EVENT_MAINTENANCE_MODE  (1 << 6)

//=============================================================================
// Public Functions
//=============================================================================

system_state_manager_t* system_state_init(void);
void system_state_deinit(system_state_manager_t* manager);

// State management
void system_state_set(system_state_manager_t* manager, system_state_t state);
system_state_t system_state_get(system_state_manager_t* manager);
const char* system_state_to_string(system_state_t state);
bool system_state_is_running(system_state_manager_t* manager);
bool system_state_is_ready(system_state_manager_t* manager);
bool system_state_is_alarm(system_state_manager_t* manager);

// Transitions
bool system_state_start(system_state_manager_t* manager);
bool system_state_stop(system_state_manager_t* manager);
bool system_state_trigger_alarm(system_state_manager_t* manager, alarm_code_t code, 
                                 uint8_t channel, const char* msg);
bool system_state_reset_alarm(system_state_manager_t* manager);
bool system_state_emergency_stop(system_state_manager_t* manager);

// Statistics and info
uint32_t system_state_get_alarm_count(system_state_manager_t* manager);
uint64_t system_state_get_runtime(system_state_manager_t* manager);
const alarm_info_t* system_state_get_last_alarm(system_state_manager_t* manager);
void system_state_reset_statistics(system_state_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_STATE_H