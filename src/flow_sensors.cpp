#include "flow_sensors.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "FLOW_SENSORS";
static flow_sensor_manager_t* global_manager = NULL;

//=============================================================================
// ISR Handlers - используем atomic операции
//=============================================================================

static void IRAM_ATTR flow_isr_0(void* arg) {
    if (global_manager && global_manager->sensor_count > 0) {
        __atomic_add_fetch(&global_manager->sensors[0].pulse_count, 1, __ATOMIC_RELAXED);
        global_manager->sensors[0].last_pulse_time = esp_timer_get_time() / 1000;
    }
}

static void IRAM_ATTR flow_isr_1(void* arg) {
    if (global_manager && global_manager->sensor_count > 1) {
        __atomic_add_fetch(&global_manager->sensors[1].pulse_count, 1, __ATOMIC_RELAXED);
        global_manager->sensors[1].last_pulse_time = esp_timer_get_time() / 1000;
    }
}

static void IRAM_ATTR flow_isr_2(void* arg) {
    if (global_manager && global_manager->sensor_count > 2) {
        __atomic_add_fetch(&global_manager->sensors[2].pulse_count, 1, __ATOMIC_RELAXED);
        global_manager->sensors[2].last_pulse_time = esp_timer_get_time() / 1000;
    }
}

static void IRAM_ATTR flow_isr_3(void* arg) {
    if (global_manager && global_manager->sensor_count > 3) {
        __atomic_add_fetch(&global_manager->sensors[3].pulse_count, 1, __ATOMIC_RELAXED);
        global_manager->sensors[3].last_pulse_time = esp_timer_get_time() / 1000;
    }
}

static void IRAM_ATTR flow_isr_4(void* arg) {
    if (global_manager && global_manager->sensor_count > 4) {
        __atomic_add_fetch(&global_manager->sensors[4].pulse_count, 1, __ATOMIC_RELAXED);
        global_manager->sensors[4].last_pulse_time = esp_timer_get_time() / 1000;
    }
}

static void IRAM_ATTR flow_isr_5(void* arg) {
    if (global_manager && global_manager->sensor_count > 5) {
        __atomic_add_fetch(&global_manager->sensors[5].pulse_count, 1, __ATOMIC_RELAXED);
        global_manager->sensors[5].last_pulse_time = esp_timer_get_time() / 1000;
    }
}

static void (*isr_handlers[6])(void*) = {
    flow_isr_0, flow_isr_1, flow_isr_2,
    flow_isr_3, flow_isr_4, flow_isr_5
};

//=============================================================================
// Implementation
//=============================================================================

flow_sensor_manager_t* flow_sensors_init(const gpio_num_t* pins, uint8_t count, 
                                          float pulses_per_liter) {
    if (count > 6 || count == 0) {
        ESP_LOGE(TAG, "Invalid sensor count: %d", count);
        return NULL;
    }
    
    flow_sensor_manager_t* manager = (flow_sensor_manager_t*)malloc(sizeof(flow_sensor_manager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate flow sensor manager");
        return NULL;
    }
    
    memset(manager, 0, sizeof(flow_sensor_manager_t));
    
    manager->sensor_count = count;
    manager->pulses_per_liter = pulses_per_liter;
    manager->timeout_ms = FLOW_TIMEOUT_MS;
    manager->initialized = true;
    manager->alarm_count = 0;
    
    manager->mutex = xSemaphoreCreateMutex();
    if (!manager->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(manager);
        return NULL;
    }
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    for (int i = 0; i < count; i++) {
        manager->sensors[i].pin = pins[i];
        manager->sensors[i].type = FLOW_SENSOR_TYPE_HALL;
        manager->sensors[i].pulse_count = 0;
        manager->sensors[i].last_pulse_time = 0;
        manager->sensors[i].last_read_time = 0;
        manager->sensors[i].flow_rate = 0;
        manager->sensors[i].total_flow = 0;
        manager->sensors[i].flowing = false;
        manager->sensors[i].alarm = false;
        manager->sensors[i].status = FLOW_STATUS_INACTIVE;
        manager->sensors[i].error_count = 0;
        
        // Configure GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pins[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        
        esp_err_t ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure GPIO for sensor %d: %d", i, ret);
            manager->sensors[i].status = FLOW_STATUS_ERROR;
            manager->sensors[i].error_count++;
        }
        
        // Add ISR
        ret = gpio_isr_handler_add(pins[i], isr_handlers[i], NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add ISR for sensor %d: %d", i, ret);
            manager->sensors[i].status = FLOW_STATUS_ERROR;
            manager->sensors[i].error_count++;
        } else {
            manager->sensors[i].status = FLOW_STATUS_OK;
        }
    }
    
    global_manager = manager;
    ESP_LOGI(TAG, "Flow sensors initialized: %d sensors, %.2f pulses/L", 
             count, pulses_per_liter);
    return manager;
}

void flow_sensors_deinit(flow_sensor_manager_t* manager) {
    if (!manager) return;
    
    for (int i = 0; i < manager->sensor_count; i++) {
        gpio_isr_handler_remove(manager->sensors[i].pin);
    }
    
    if (manager->mutex) {
        vSemaphoreDelete(manager->mutex);
    }
    
    free(manager);
}

void flow_sensors_update(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return;
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    manager->alarm_count = 0;
    
    for (int i = 0; i < manager->sensor_count; i++) {
        flow_sensor_t* sensor = &manager->sensors[i];
        
        // Check flow status
        if (current_time - sensor->last_pulse_time > manager->timeout_ms) {
            if (sensor->flowing) {
                sensor->flowing = false;
                sensor->alarm = true;
                sensor->status = FLOW_STATUS_ALARM;
                manager->alarm_sensors[manager->alarm_count++] = i;
                ESP_LOGW(TAG, "Flow lost on sensor %d", i + 1);
            }
        } else {
            if (!sensor->flowing) {
                sensor->flowing = true;
                sensor->alarm = false;
                sensor->status = FLOW_STATUS_OK;
                ESP_LOGI(TAG, "Flow restored on sensor %d", i + 1);
            }
        }
        
        // Calculate flow rate (L/min)
        if (current_time - sensor->last_read_time > FLOW_CALC_INTERVAL_MS) {
            uint32_t pulses;
            // Используем atomic для чтения
            pulses = __atomic_exchange_n(&sensor->pulse_count, 0, __ATOMIC_RELAXED);
            
            // pulses per second * 60 / pulses per liter = L/min
            float rate = (pulses * 60.0f) / manager->pulses_per_liter;
            sensor->flow_rate = rate;
            sensor->total_flow += rate / 60.0f;
            sensor->last_read_time = current_time;
            
            ESP_LOGD(TAG, "Sensor %d: %d pulses, flow: %.2f L/min", 
                     i + 1, pulses, rate);
        }
    }
    
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
}

bool flow_sensor_is_flowing(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) return false;
    
    bool flowing;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    flowing = manager->sensors[index].flowing;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    return flowing;
}

bool flow_sensors_all_flowing(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return false;
    
    bool all_flowing = true;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    for (int i = 0; i < manager->sensor_count; i++) {
        if (!manager->sensors[i].flowing) {
            all_flowing = false;
            break;
        }
    }
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    return all_flowing;
}

bool flow_sensors_any_alarm(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return false;
    
    bool has_alarm = false;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    has_alarm = (manager->alarm_count > 0);
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    return has_alarm;
}

float flow_sensor_get_rate(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) return 0;
    
    float rate;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    rate = manager->sensors[index].flow_rate;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    return rate;
}

float flow_sensor_get_total(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) return 0;
    
    float total;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    total = manager->sensors[index].total_flow;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    return total;
}

flow_status_t flow_sensor_get_status(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) {
        return FLOW_STATUS_ERROR;
    }
    
    flow_status_t status;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    status = manager->sensors[index].status;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    return status;
}

void flow_sensors_get_alarms(flow_sensor_manager_t* manager, uint8_t* indices, uint8_t* count) {
    if (!manager || !indices || !count) return;
    
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    *count = manager->alarm_count;
    memcpy(indices, manager->alarm_sensors, manager->alarm_count);
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
}

void flow_sensors_reset(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return;
    
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    for (int i = 0; i < manager->sensor_count; i++) {
        manager->sensors[i].alarm = false;
        manager->sensors[i].flowing = false;
        manager->sensors[i].pulse_count = 0;
        manager->sensors[i].last_pulse_time = esp_timer_get_time() / 1000;
        manager->sensors[i].status = FLOW_STATUS_OK;
    }
    manager->alarm_count = 0;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    ESP_LOGI(TAG, "Flow sensors reset");
}

void flow_sensors_clear_alarm(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) return;
    
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    manager->sensors[index].alarm = false;
    manager->sensors[index].status = FLOW_STATUS_OK;
    // Rebuild alarm list
    manager->alarm_count = 0;
    for (int i = 0; i < manager->sensor_count; i++) {
        if (manager->sensors[i].alarm) {
            manager->alarm_sensors[manager->alarm_count++] = i;
        }
    }
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
}

void flow_sensors_format_display(flow_sensor_manager_t* manager, char* line1, 
                                  char* line2, uint8_t max_len) {
    if (!manager || !line1 || !line2) return;
    
    if (manager->sensor_count == 1) {
        bool flowing = flow_sensor_is_flowing(manager, 0);
        float rate = flow_sensor_get_rate(manager, 0);
        snprintf(line1, max_len, "CH1:%s", flowing ? " ALARM!" : " OK");
        snprintf(line2, max_len, "Rate: %.1fL/min", rate);
    } else {
        // Multi-sensor format
        snprintf(line1, max_len, "CH1:%.1f CH2:%.1f", 
                 flow_sensor_get_rate(manager, 0),
                 flow_sensor_get_rate(manager, 1));
        if (manager->sensor_count > 2) {
            snprintf(line2, max_len, "CH3:%.1f CH4:%.1f",
                     flow_sensor_get_rate(manager, 2),
                     flow_sensor_get_rate(manager, 3));
        }
    }
}

void flow_sensors_format_alarms(flow_sensor_manager_t* manager, char* buffer, uint8_t max_len) {
    if (!manager || !buffer) return;
    
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    if (manager->alarm_count == 0) {
        snprintf(buffer, max_len, "No alarms");
    } else {
        char* ptr = buffer;
        for (int i = 0; i < manager->alarm_count && i < 6; i++) {
            if (i > 0) {
                ptr += snprintf(ptr, max_len - (ptr - buffer), ",");
            }
            ptr += snprintf(ptr, max_len - (ptr - buffer), "CH%d", 
                          manager->alarm_sensors[i] + 1);
        }
    }
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
}

void flow_sensors_set_timeout(flow_sensor_manager_t* manager, uint32_t timeout_ms) {
    if (!manager) return;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    manager->timeout_ms = timeout_ms;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
}

void flow_sensors_set_calibration(flow_sensor_manager_t* manager, float pulses_per_liter) {
    if (!manager) return;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    manager->pulses_per_liter = pulses_per_liter;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
}

uint32_t flow_sensors_get_error_count(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) return 0;
    
    uint32_t count;
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    count = manager->sensors[index].error_count;
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    return count;
}

void flow_sensors_reset_error_counts(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return;
    
    if (manager->mutex) {
        xSemaphoreTake(manager->mutex, portMAX_DELAY);
    }
    for (int i = 0; i < manager->sensor_count; i++) {
        manager->sensors[i].error_count = 0;
    }
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
}