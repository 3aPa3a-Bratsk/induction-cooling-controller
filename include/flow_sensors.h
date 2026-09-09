#ifndef FLOW_SENSORS_H
#define FLOW_SENSORS_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Flow Sensor Types
//=============================================================================

typedef enum {
    FLOW_SENSOR_TYPE_HALL = 0,
    FLOW_SENSOR_TYPE_REED,
    FLOW_SENSOR_TYPE_OPTICAL
} flow_sensor_type_t;

typedef enum {
    FLOW_STATUS_OK = 0,
    FLOW_STATUS_ALARM,
    FLOW_STATUS_ERROR,
    FLOW_STATUS_INACTIVE
} flow_status_t;

typedef struct {
    gpio_num_t pin;
    flow_sensor_type_t type;
    volatile uint32_t pulse_count;
    volatile uint32_t last_pulse_time;
    uint32_t last_read_time;
    float flow_rate;
    float total_flow;
    bool flowing;
    bool alarm;
    flow_status_t status;
    uint32_t error_count;
} flow_sensor_t;

typedef struct {
    flow_sensor_t sensors[6];
    uint8_t sensor_count;
    float pulses_per_liter;
    uint32_t timeout_ms;
    bool initialized;
    uint8_t alarm_sensors[6];
    uint8_t alarm_count;
    SemaphoreHandle_t mutex;
} flow_sensor_manager_t;

//=============================================================================
// Public Functions
//=============================================================================

flow_sensor_manager_t* flow_sensors_init(const gpio_num_t* pins, uint8_t count, 
                                          float pulses_per_liter);
void flow_sensors_deinit(flow_sensor_manager_t* manager);
void flow_sensors_update(flow_sensor_manager_t* manager);
bool flow_sensor_is_flowing(flow_sensor_manager_t* manager, uint8_t index);
bool flow_sensors_all_flowing(flow_sensor_manager_t* manager);
bool flow_sensors_any_alarm(flow_sensor_manager_t* manager);
float flow_sensor_get_rate(flow_sensor_manager_t* manager, uint8_t index);
float flow_sensor_get_total(flow_sensor_manager_t* manager, uint8_t index);
flow_status_t flow_sensor_get_status(flow_sensor_manager_t* manager, uint8_t index);
void flow_sensors_get_alarms(flow_sensor_manager_t* manager, uint8_t* indices, uint8_t* count);
void flow_sensors_reset(flow_sensor_manager_t* manager);
void flow_sensors_clear_alarm(flow_sensor_manager_t* manager, uint8_t index);
void flow_sensors_format_display(flow_sensor_manager_t* manager, char* line1, 
                                  char* line2, uint8_t max_len);
void flow_sensors_format_alarms(flow_sensor_manager_t* manager, char* buffer, 
                                uint8_t max_len);
void flow_sensors_set_timeout(flow_sensor_manager_t* manager, uint32_t timeout_ms);
void flow_sensors_set_calibration(flow_sensor_manager_t* manager, float pulses_per_liter);
uint32_t flow_sensors_get_error_count(flow_sensor_manager_t* manager, uint8_t index);
void flow_sensors_reset_error_counts(flow_sensor_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif // FLOW_SENSORS_H