#include "flow_sensors.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "FLOW_SENSORS";
static flow_sensor_manager_t* global_manager = NULL;

//=============================================================================
// ISR для 6 датчиков - используем атомарные операции
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
// Инициализация
//=============================================================================
flow_sensor_manager_t* flow_sensors_init(const gpio_num_t* pins, uint8_t count, 
                                          float pulses_per_liter) {
    if (count > 6 || count == 0) {
        ESP_LOGE(TAG, "Invalid sensor count: %d", count);
        return NULL;
    }
    
    flow_sensor_manager_t* manager = (flow_sensor_manager_t*)malloc(sizeof(flow_sensor_manager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate");
        return NULL;
    }
    
    memset(manager, 0, sizeof(flow_sensor_manager_t));
    
    manager->sensor_count = count;
    manager->pulses_per_liter = pulses_per_liter;
    manager->timeout_ms = FLOW_TIMEOUT_MS;
    manager->initialized = true;
    manager->alarm_count = 0;
    manager->mutex = NULL;
    
    gpio_install_isr_service(0);
    
    for (int i = 0; i < count; i++) {
        manager->sensors[i].pin = pins[i];
        manager->sensors[i].pulse_count = 0;
        manager->sensors[i].last_pulse_time = 0;
        manager->sensors[i].last_read_time = 0;
        manager->sensors[i].flow_rate = 0;
        manager->sensors[i].total_flow = 0;
        manager->sensors[i].flowing = false;
        manager->sensors[i].alarm = false;
        manager->sensors[i].status = FLOW_STATUS_INACTIVE;
        manager->sensors[i].error_count = 0;
        
        // Нормальная логика: прерывание по спаду
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pins[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        
        gpio_config(&io_conf);
        gpio_isr_handler_add(pins[i], isr_handlers[i], NULL);
        
        int level = gpio_get_level(pins[i]);
        ESP_LOGI(TAG, "Sensor %d: pin=%d, level=%d", i + 1, pins[i], level);
    }
    
    global_manager = manager;
    ESP_LOGI(TAG, "Flow sensors initialized: %d sensors", count);
    return manager;
}

//=============================================================================
// Обновление датчиков
//=============================================================================
void flow_sensors_update(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return;
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    manager->alarm_count = 0;
    
    for (int i = 0; i < manager->sensor_count; i++) {
        flow_sensor_t* sensor = &manager->sensors[i];
        
        uint32_t time_since_last = current_time - sensor->last_pulse_time;
        bool has_pulses = (time_since_last < manager->timeout_ms);
        
        // НОРМАЛЬНАЯ ЛОГИКА: есть импульсы = поток есть
        if (has_pulses) {
            if (!sensor->flowing) {
                sensor->flowing = true;
                sensor->alarm = false;
                sensor->status = FLOW_STATUS_OK;
                ESP_LOGI(TAG, "✅ Flow detected on sensor %d", i + 1);
            }
        } else {
            if (sensor->flowing) {
                sensor->flowing = false;
                sensor->alarm = true;
                sensor->status = FLOW_STATUS_ALARM;
                manager->alarm_sensors[manager->alarm_count++] = i;
                ESP_LOGW(TAG, "❌ Flow lost on sensor %d", i + 1);
            }
        }
        
        // Расчет расхода (для справки, на экран не выводим)
        if (current_time - sensor->last_read_time > FLOW_CALC_INTERVAL_MS) {
            // Атомарное чтение и сброс
            uint32_t pulses = __atomic_exchange_n(&sensor->pulse_count, 0, __ATOMIC_RELAXED);
            sensor->flow_rate = (pulses * 60.0f) / manager->pulses_per_liter;
            sensor->total_flow += sensor->flow_rate / 60.0f;
            sensor->last_read_time = current_time;
        }
    }
}

bool flow_sensor_is_flowing(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) return false;
    return manager->sensors[index].flowing;
}

bool flow_sensors_all_flowing(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return false;
    for (int i = 0; i < manager->sensor_count; i++) {
        if (!manager->sensors[i].flowing) return false;
    }
    return true;
}

bool flow_sensors_any_alarm(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return false;
    return manager->alarm_count > 0;
}

float flow_sensor_get_rate(flow_sensor_manager_t* manager, uint8_t index) {
    if (!manager || !manager->initialized || index >= manager->sensor_count) return 0;
    return manager->sensors[index].flow_rate;
}

void flow_sensors_reset(flow_sensor_manager_t* manager) {
    if (!manager || !manager->initialized) return;
    for (int i = 0; i < manager->sensor_count; i++) {
        manager->sensors[i].alarm = false;
        manager->sensors[i].flowing = false;
        manager->sensors[i].pulse_count = 0;
        manager->sensors[i].last_pulse_time = esp_timer_get_time() / 1000;
        manager->sensors[i].status = FLOW_STATUS_OK;
    }
    manager->alarm_count = 0;
    ESP_LOGI(TAG, "Flow sensors reset");
}

void flow_sensors_get_alarms(flow_sensor_manager_t* manager, uint8_t* indices, uint8_t* count) {
    if (!manager || !indices || !count) return;
    *count = manager->alarm_count;
    memcpy(indices, manager->alarm_sensors, manager->alarm_count);
}