#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "config.h"
#include "version.h"
#include "i2c_manager.h"
#include "system_state.h"
#include "lcd1602.h"
#include "relay.h"
#include "buttons.h"
#include "flow_sensors.h"
#include "watchdog.h"

static const char *TAG = "MAIN";

static i2c_manager_t i2c_manager;
static system_state_manager_t* system_manager = NULL;
static lcd1602_t* lcd = NULL;
static relay_manager_t* relay_manager = NULL;
static button_manager_t* button_manager = NULL;
static flow_sensor_manager_t* flow_manager = NULL;
static watchdog_manager_t* watchdog = NULL;

static EventGroupHandle_t main_event_group = NULL;

#define EVENT_BUTTON_START   (1 << 0)
#define EVENT_BUTTON_STOP    (1 << 1)
#define EVENT_BUTTON_RESET   (1 << 2)
#define EVENT_FLOW_ALARM     (1 << 3)
#define EVENT_UPDATE_DISPLAY (1 << 4)

static bool last_start = false, last_stop = false, last_reset = false;

static char display_line1[17] = "                ";
static char display_line2[17] = "                ";
static system_state_t last_display_state = STATE_READY;

// Массив пинов для диагностики
static gpio_num_t flow_pins_array[6] = {
    FLOW_SENSOR_1, FLOW_SENSOR_2, FLOW_SENSOR_3,
    FLOW_SENSOR_4, FLOW_SENSOR_5, FLOW_SENSOR_6
};

//=============================================================================
// Получение списка каналов с ошибкой (только цифры через запятую)
//=============================================================================
static void get_failed_channels(char* buffer, size_t size) {
    buffer[0] = '\0';
    int count = 0;
    
    for (int i = 0; i < 6; i++) {
        bool flowing = flow_sensor_is_flowing(flow_manager, i);
        if (!flowing) {
            if (count > 0) {
                strncat(buffer, ",", size - strlen(buffer) - 1);
            }
            char num[4];
            snprintf(num, sizeof(num), "%d", i + 1);
            strncat(buffer, num, size - strlen(buffer) - 1);
            count++;
        }
    }
    
    if (count == 0) {
        strncpy(buffer, "-", size);
    }
}

//=============================================================================
// ДИАГНОСТИКА: вывод состояния всех датчиков
//=============================================================================
static void print_flow_diagnostics(void) {
    if (!flow_manager) return;
    
    char status[128] = "";
    char level_str[128] = "";
    char failed[32] = "";
    
    get_failed_channels(failed, sizeof(failed));
    
    for (int i = 0; i < 6; i++) {
        bool flowing = flow_sensor_is_flowing(flow_manager, i);
        int level = gpio_get_level(flow_pins_array[i]);
        uint32_t pulses = flow_manager->sensors[i].pulse_count;
        
        char buf[20];
        snprintf(buf, sizeof(buf), "%d:%s ", i + 1, flowing ? "OK" : "NO");
        strcat(status, buf);
        
        char lvl_buf[20];
        snprintf(lvl_buf, sizeof(lvl_buf), "%d=%d P=%lu ", i + 1, level, (unsigned long)pulses);
        strcat(level_str, lvl_buf);
    }
    
    ESP_LOGI(TAG, "📊 FLOW: %s", status);
    ESP_LOGI(TAG, "🔍 LEVELS: %s", level_str);
    if (strlen(failed) > 0 && strcmp(failed, "-") != 0) {
        ESP_LOGW(TAG, "⚠️ ERR: %s", failed);
    }
}

//=============================================================================
// Формирование строк для дисплея
//=============================================================================
static void format_display_strings(system_state_t state, char* line1, char* line2) {
    // Проверяем наличие потока на всех каналах (постоянный мониторинг)
    bool all_flowing = flow_sensors_all_flowing(flow_manager);
    char failed[32] = "";
    get_failed_channels(failed, sizeof(failed));
    
    switch (state) {
        case STATE_READY:
            if (all_flowing) {
                snprintf(line1, 16, "System ready.");
                snprintf(line2, 16, "Press start.");
            } else {
                snprintf(line1, 16, "! NOT ready !");
                snprintf(line2, 16, "ERR:%s", failed);
            }
            break;
            
        case STATE_RUNNING: {
            char ch1[6], ch2[6], ch3[6], ch4[6], ch5[6], ch6[6];
            
            snprintf(ch1, 6, "%s", flow_sensor_is_flowing(flow_manager, 0) ? "OK" : "!!!");
            snprintf(ch2, 6, "%s", flow_sensor_is_flowing(flow_manager, 1) ? "OK" : "!!!");
            snprintf(ch3, 6, "%s", flow_sensor_is_flowing(flow_manager, 2) ? "OK" : "!!!");
            snprintf(ch4, 6, "%s", flow_sensor_is_flowing(flow_manager, 3) ? "OK" : "!!!");
            snprintf(ch5, 6, "%s", flow_sensor_is_flowing(flow_manager, 4) ? "OK" : "!!!");
            snprintf(ch6, 6, "%s", flow_sensor_is_flowing(flow_manager, 5) ? "OK" : "!!!");
            
            snprintf(line1, 16, "1:%s 2:%s 3:%s", ch1, ch2, ch3);
            snprintf(line2, 16, "4:%s 5:%s 6:%s", ch4, ch5, ch6);
            break;
        }
            
        case STATE_ALARM:
            snprintf(line1, 16, "!! ALARM !!");
            snprintf(line2, 16, "ERR: %s", failed);
            break;
            
        default:
            snprintf(line1, 16, "Unknown state");
            snprintf(line2, 16, "Reboot");
            break;
    }
}

//=============================================================================
// Обновление дисплея
//=============================================================================
static void update_display(void) {
    if (!lcd) return;
    
    system_state_t state = system_state_get(system_manager);
    char line1[17] = "                ";
    char line2[17] = "                ";
    
    format_display_strings(state, line1, line2);
    
    bool need_update = false;
    
    if (state != last_display_state) {
        last_display_state = state;
        need_update = true;
    }
    
    if (strcmp(line1, display_line1) != 0) {
        strcpy(display_line1, line1);
        need_update = true;
    }
    
    if (strcmp(line2, display_line2) != 0) {
        strcpy(display_line2, line2);
        need_update = true;
    }
    
    if (need_update) {
        lcd1602_print(lcd, line1, line2);
    }
}

//=============================================================================
// Main
//=============================================================================
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "%s v%s", FIRMWARE_NAME, FIRMWARE_VERSION);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Induction Furnace Cooling System");
    ESP_LOGI(TAG, "6 Flow Sensors - 1 sec timeout");
    ESP_LOGI(TAG, "Pins: 10,11,20,21,22,23");
    ESP_LOGI(TAG, "========================================");
    
    nvs_flash_init();
    
    main_event_group = xEventGroupCreate();
    system_manager = system_state_init();
    
    i2c_manager_init(&i2c_manager);
    
    lcd = lcd1602_init(LCD_ADDRESS, LCD_COLS, LCD_ROWS, &i2c_manager);
    
    relay_manager = relay_init(RELAY_EXPANDER_ADDR, &i2c_manager);
    if (relay_manager) {
        relay_set(relay_manager, RELAY_MAIN, false);
        relay_set(relay_manager, RELAY_READY, true);
        relay_set(relay_manager, RELAY_BEACON, false);
        relay_set(relay_manager, RELAY_POWER_LAMP, true);
    }
    
    button_manager = buttons_init(BUTTON_EXPANDER_ADDR, &i2c_manager);
    buttons_update(button_manager);
    
    gpio_num_t flow_pins[] = {
        FLOW_SENSOR_1, FLOW_SENSOR_2, FLOW_SENSOR_3,
        FLOW_SENSOR_4, FLOW_SENSOR_5, FLOW_SENSOR_6
    };
    flow_manager = flow_sensors_init(flow_pins, FLOW_SENSORS_COUNT, FLOW_PULSES_PER_LITER);
    
    watchdog = watchdog_init(WATCHDOG_TIMEOUT_SEC, WATCHDOG_FEED_INTERVAL_MS);
    
    update_display();
    
    ESP_LOGI(TAG, "System ready - 6-channel cooling monitoring");
    
    // Первичная диагностика через 2 секунды
    vTaskDelay(pdMS_TO_TICKS(2000));
    print_flow_diagnostics();
    
    while (1) {
        watchdog_feed(watchdog);
        
        if (buttons_update(button_manager)) {
            bool start = button_is_start_pressed(button_manager);
            bool stop = button_is_stop_pressed(button_manager);
            bool reset = button_is_reset_pressed(button_manager);
            
            if (start && !last_start) {
                xEventGroupSetBits(main_event_group, EVENT_BUTTON_START);
            }
            if (stop && !last_stop) {
                xEventGroupSetBits(main_event_group, EVENT_BUTTON_STOP);
            }
            if (reset && !last_reset) {
                xEventGroupSetBits(main_event_group, EVENT_BUTTON_RESET);
            }
            
            last_start = start;
            last_stop = stop;
            last_reset = reset;
        }
        
        EventBits_t bits = xEventGroupWaitBits(
            main_event_group,
            EVENT_BUTTON_START | EVENT_BUTTON_STOP | EVENT_BUTTON_RESET | 
            EVENT_FLOW_ALARM | EVENT_UPDATE_DISPLAY,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(50)
        );
        
        if (bits) {
            system_state_t state = system_state_get(system_manager);
            
            if (bits & EVENT_BUTTON_START) {
                if (state == STATE_READY) {
                    print_flow_diagnostics();
                    
                    bool all_flowing = flow_sensors_all_flowing(flow_manager);
                    if (all_flowing) {
                        system_state_set(system_manager, STATE_RUNNING);
                        relay_set(relay_manager, RELAY_MAIN, true);
                        relay_set(relay_manager, RELAY_READY, false);
                        relay_set(relay_manager, RELAY_BEACON, true);
                        ESP_LOGI(TAG, "✅ SYSTEM STARTED - All flows OK");
                        update_display();
                    } else {
                        char failed[32] = "";
                        get_failed_channels(failed, sizeof(failed));
                        ESP_LOGW(TAG, "❌ START DENIED - ERR: %s", failed);
                        lcd1602_print(lcd, "!! DENIED !!", failed);
                        vTaskDelay(pdMS_TO_TICKS(1500));
                        update_display();
                    }
                }
            }
            
            if (bits & EVENT_BUTTON_STOP) {
                if (state == STATE_RUNNING) {
                    system_state_set(system_manager, STATE_READY);
                    relay_set(relay_manager, RELAY_MAIN, false);
                    relay_set(relay_manager, RELAY_READY, true);
                    relay_set(relay_manager, RELAY_BEACON, false);
                    ESP_LOGI(TAG, "✅ SYSTEM STOPPED");
                    update_display();
                }
            }
            
            if (bits & EVENT_BUTTON_RESET) {
                if (state == STATE_ALARM) {
                    system_state_set(system_manager, STATE_READY);
                    relay_set(relay_manager, RELAY_MAIN, false);
                    relay_set(relay_manager, RELAY_READY, true);
                    relay_set(relay_manager, RELAY_BEACON, false);
                    flow_sensors_reset(flow_manager);
                    ESP_LOGI(TAG, "✅ ALARM RESET");
                    print_flow_diagnostics();
                    update_display();
                }
            }
            
            if (bits & EVENT_FLOW_ALARM) {
                if (state == STATE_RUNNING && !system_state_is_alarm(system_manager)) {
                    char failed[32] = "";
                    get_failed_channels(failed, sizeof(failed));
                    
                    system_state_trigger_alarm(system_manager, ALARM_FLOW_LOSS, 0, failed);
                    relay_set(relay_manager, RELAY_MAIN, false);
                    relay_set(relay_manager, RELAY_READY, true);
                    relay_set(relay_manager, RELAY_BEACON, true);
                    ESP_LOGE(TAG, "🚨 CRITICAL: ERR: %s", failed);
                    print_flow_diagnostics();
                    update_display();
                }
            }
            
            if (bits & EVENT_UPDATE_DISPLAY) {
                update_display();
            }
        }
        
        // Фоновое обновление датчиков (каждые 500ms)
        static uint32_t flow_timer = 0;
        flow_timer++;
        if (flow_timer % 5 == 0) {
            flow_timer = 0;
            flow_sensors_update(flow_manager);
            
            if (flow_sensors_any_alarm(flow_manager)) {
                if (system_state_is_running(system_manager)) {
                    xEventGroupSetBits(main_event_group, EVENT_FLOW_ALARM);
                }
            }
            
            update_display();
            
            static uint32_t log_timer = 0;
            log_timer++;
            if (log_timer % 6 == 0) {
                print_flow_diagnostics();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}