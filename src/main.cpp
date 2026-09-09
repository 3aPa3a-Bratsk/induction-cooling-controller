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
#include "driver/i2c.h"
#include "esp_timer.h"

#include "config.h"
#include "version.h"
#include "system_state.h"
#include "lcd1602.h"
#include "relay.h"
#include "buttons.h"
#include "flow_sensors.h"
#include "watchdog.h"

static const char *TAG = "MAIN";

//=============================================================================
// Global Instances
//=============================================================================

static system_state_manager_t* system_manager = NULL;
static lcd1602_t* lcd = NULL;
static relay_manager_t* relay_manager = NULL;
static button_manager_t* button_manager = NULL;
static flow_sensor_manager_t* flow_manager = NULL;
static watchdog_manager_t* watchdog = NULL;

// Task handles
static TaskHandle_t control_task_handle = NULL;
static TaskHandle_t display_task_handle = NULL;
static TaskHandle_t button_task_handle = NULL;
static TaskHandle_t flow_task_handle = NULL;

// Event group for inter-task communication
static EventGroupHandle_t main_event_group = NULL;

// Event bits
#define EVENT_BUTTON_START   (1 << 0)
#define EVENT_BUTTON_STOP    (1 << 1)
#define EVENT_BUTTON_RESET   (1 << 2)
#define EVENT_FLOW_ALARM     (1 << 3)
#define EVENT_FLOW_OK        (1 << 4)
#define EVENT_UPDATE_DISPLAY (1 << 5)
#define EVENT_WATCHDOG       (1 << 6)

// Button debounce variables
static bool last_start_state = false;
static bool last_stop_state = false;
static bool last_reset_state = false;

//=============================================================================
// Forward Declarations
//=============================================================================

static void control_task(void *pvParameters);
static void display_task(void *pvParameters);
static void button_task(void *pvParameters);
static void flow_task(void *pvParameters);
static void update_display(void);

//=============================================================================
// Main Entry Point
//=============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "%s v%s", FIRMWARE_NAME, FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Build: %s %s", FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME);
    ESP_LOGI(TAG, "Model: %s", SYSTEM_MODEL);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Industrial Cooling Controller");
    ESP_LOGI(TAG, "ESP32-C6 - Test Mode (Single Sensor)");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create main event group
    main_event_group = xEventGroupCreate();
    if (!main_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }
    
    // Initialize system state
    system_manager = system_state_init();
    if (!system_manager) {
        ESP_LOGE(TAG, "Failed to initialize system state");
        return;
    }
    
    // Initialize I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ret = i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %d", ret);
        return;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %d", ret);
        return;
    }
    
    // Initialize LCD
    lcd = lcd1602_init(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
    if (!lcd) {
        ESP_LOGE(TAG, "Failed to initialize LCD");
        return;
    }
    
    // Initialize Relay Manager
    relay_manager = relay_init(RELAY_EXPANDER_ADDR);
    if (!relay_manager) {
        ESP_LOGE(TAG, "Failed to initialize relay manager");
        return;
    }
    
    // Set initial relay states
    relay_set(relay_manager, RELAY_MAIN, false);        // Main contactor OFF
    relay_set(relay_manager, RELAY_READY, true);        // Ready indicator ON
    relay_set(relay_manager, RELAY_BEACON, false);      // Beacon OFF
    relay_set(relay_manager, RELAY_POWER_LAMP, true);   // Power lamp ON
    
    // Initialize Buttons
    button_manager = buttons_init(BUTTON_EXPANDER_ADDR);
    if (!button_manager) {
        ESP_LOGE(TAG, "Failed to initialize button manager");
        return;
    }
    
    // Initialize Flow Sensors
    gpio_num_t flow_pins[] = {FLOW_SENSOR_1};
    flow_manager = flow_sensors_init(flow_pins, FLOW_SENSORS_ACTIVE, FLOW_PULSES_PER_LITER);
    if (!flow_manager) {
        ESP_LOGE(TAG, "Failed to initialize flow sensors");
        return;
    }
    
    // Initialize Watchdog
    watchdog = watchdog_init(WATCHDOG_TIMEOUT_SEC, WATCHDOG_FEED_INTERVAL_MS);
    if (!watchdog) {
        ESP_LOGE(TAG, "Failed to initialize watchdog");
        return;
    }
    
    // Read initial button states
    buttons_update(button_manager);
    last_start_state = button_is_start_pressed(button_manager);
    last_stop_state = button_is_stop_pressed(button_manager);
    last_reset_state = button_is_reset_pressed(button_manager);
    
    // Show initial state
    lcd1602_print(lcd, "System ready.", "Press start.");
    ESP_LOGI(TAG, "System initialized in READY state");
    
    // Create tasks
    xTaskCreate(control_task, "control_task", TASK_STACK_CONTROL, NULL, 
                TASK_PRIORITY_CONTROL, &control_task_handle);
    xTaskCreate(display_task, "display_task", TASK_STACK_DISPLAY, NULL, 
                TASK_PRIORITY_DISPLAY, &display_task_handle);
    xTaskCreate(button_task, "button_task", TASK_STACK_BUTTONS, NULL, 
                TASK_PRIORITY_BUTTONS, &button_task_handle);
    xTaskCreate(flow_task, "flow_task", TASK_STACK_FLOW, NULL, 
                TASK_PRIORITY_FLOW, &flow_task_handle);
    
    ESP_LOGI(TAG, "All tasks started");
    
    // Main loop - feed watchdog and handle events
    while (1) {
        // Feed watchdog
        watchdog_feed(watchdog);
        
        // Wait for events
        EventBits_t bits = xEventGroupWaitBits(
            main_event_group,
            EVENT_FLOW_ALARM | EVENT_FLOW_OK | EVENT_UPDATE_DISPLAY,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(100)
        );
        
        // Update flow sensors periodically
        flow_sensors_update(flow_manager);
        
        // Check flow status (INVERTED LOGIC FOR TESTING)
        // Normal: no flow = alarm, flow = OK
        // INVERTED: flow = alarm, no flow = OK
        if (system_state_is_running(system_manager)) {
            bool sensor_flowing = flow_sensor_is_flowing(flow_manager, 0);
            
            // INVERTED LOGIC: If sensor is flowing -> ALARM
            if (sensor_flowing) {
                ESP_LOGW(TAG, "!!! FLOW DETECTED - ALARM TRIGGERED (TEST MODE) !!!");
                system_state_trigger_alarm(system_manager, ALARM_FLOW_INVERTED, 
                                          0, "Flow detected during test");
                relay_set(relay_manager, RELAY_MAIN, false);
                relay_set(relay_manager, RELAY_READY, true);
                relay_set(relay_manager, RELAY_BEACON, true);
                xEventGroupSetBits(main_event_group, EVENT_FLOW_ALARM);
                xEventGroupSetBits(main_event_group, EVENT_UPDATE_DISPLAY);
            } else {
                // No flow - normal operation
                xEventGroupSetBits(main_event_group, EVENT_FLOW_OK);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

//=============================================================================
// Control Task
//=============================================================================

static void control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Control task started");
    
    while (1) {
        // Wait for button events
        EventBits_t bits = xEventGroupWaitBits(
            main_event_group,
            EVENT_BUTTON_START | EVENT_BUTTON_STOP | EVENT_BUTTON_RESET,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        
        // Check current state
        system_state_t current_state = system_state_get(system_manager);
        
        // Handle START button
        if (bits & EVENT_BUTTON_START) {
            if (current_state == STATE_READY) {
                // Check if flow is OK (INVERTED: no flow = OK for test)
                bool sensor_flowing = flow_sensor_is_flowing(flow_manager, 0);
                
                if (!sensor_flowing) {
                    // Start system
                    system_state_set(system_manager, STATE_RUNNING);
                    relay_set(relay_manager, RELAY_MAIN, true);
                    relay_set(relay_manager, RELAY_READY, false);
                    relay_set(relay_manager, RELAY_BEACON, true);
                    
                    ESP_LOGI(TAG, "System STARTED");
                    xEventGroupSetBits(main_event_group, EVENT_UPDATE_DISPLAY);
                } else {
                    ESP_LOGW(TAG, "Cannot start - flow detected (test mode)");
                    lcd1602_print(lcd, "!! START DENIED !!", "Flow sensor active");
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    update_display();
                }
            } else {
                ESP_LOGW(TAG, "Start ignored - system in %s state", 
                         system_state_to_string(current_state));
            }
        }
        
        // Handle STOP button
        if (bits & EVENT_BUTTON_STOP) {
            if (current_state == STATE_RUNNING) {
                system_state_set(system_manager, STATE_READY);
                relay_set(relay_manager, RELAY_MAIN, false);
                relay_set(relay_manager, RELAY_READY, true);
                relay_set(relay_manager, RELAY_BEACON, false);
                
                ESP_LOGI(TAG, "System STOPPED");
                xEventGroupSetBits(main_event_group, EVENT_UPDATE_DISPLAY);
            }
        }
        
        // Handle RESET button
        if (bits & EVENT_BUTTON_RESET) {
            if (current_state == STATE_ALARM) {
                system_state_set(system_manager, STATE_READY);
                relay_set(relay_manager, RELAY_MAIN, false);
                relay_set(relay_manager, RELAY_READY, true);
                relay_set(relay_manager, RELAY_BEACON, false);
                
                // Clear flow sensor alarms
                flow_sensors_reset(flow_manager);
                
                ESP_LOGI(TAG, "Alarm RESET - System READY");
                xEventGroupSetBits(main_event_group, EVENT_UPDATE_DISPLAY);
            } else {
                ESP_LOGW(TAG, "Reset ignored - not in alarm state");
            }
        }
    }
}

//=============================================================================
// Button Task
//=============================================================================

static void button_task(void *pvParameters) {
    ESP_LOGI(TAG, "Button task started");
    
    while (1) {
        // Update button states
        if (buttons_update(button_manager)) {
            bool start_pressed = button_is_start_pressed(button_manager);
            bool stop_pressed = button_is_stop_pressed(button_manager);
            bool reset_pressed = button_is_reset_pressed(button_manager);
            
            // Detect rising edges (button press)
            if (start_pressed && !last_start_state) {
                ESP_LOGI(TAG, "START button pressed");
                xEventGroupSetBits(main_event_group, EVENT_BUTTON_START);
            }
            
            if (stop_pressed && !last_stop_state) {
                ESP_LOGI(TAG, "STOP button pressed");
                xEventGroupSetBits(main_event_group, EVENT_BUTTON_STOP);
            }
            
            if (reset_pressed && !last_reset_state) {
                ESP_LOGI(TAG, "RESET button pressed");
                xEventGroupSetBits(main_event_group, EVENT_BUTTON_RESET);
            }
            
            last_start_state = start_pressed;
            last_stop_state = stop_pressed;
            last_reset_state = reset_pressed;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//=============================================================================
// Flow Task
//=============================================================================

static void flow_task(void *pvParameters) {
    ESP_LOGI(TAG, "Flow task started");
    
    while (1) {
        // Update flow sensors
        flow_sensors_update(flow_manager);
        
        // Check for alarms
        if (flow_sensors_any_alarm(flow_manager)) {
            if (system_state_is_running(system_manager)) {
                xEventGroupSetBits(main_event_group, EVENT_FLOW_ALARM);
                xEventGroupSetBits(main_event_group, EVENT_UPDATE_DISPLAY);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//=============================================================================
// Display Task
//=============================================================================

static void display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Display task started");
    
    while (1) {
        // Wait for display update event
        xEventGroupWaitBits(
            main_event_group,
            EVENT_UPDATE_DISPLAY,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(200)
        );
        
        update_display();
    }
}

//=============================================================================
// Display Update Function
//=============================================================================

static void update_display(void) {
    system_state_t state = system_state_get(system_manager);
    char line1[17] = {0};
    char line2[17] = {0};
    
    switch (state) {
        case STATE_READY:
            snprintf(line1, sizeof(line1), "System ready.   ");
            snprintf(line2, sizeof(line2), "Press start.    ");
            break;
            
        case STATE_RUNNING: {
            // In test mode with 1 sensor
            bool flowing = flow_sensor_is_flowing(flow_manager, 0);
            float rate = flow_sensor_get_rate(flow_manager, 0);
            
            snprintf(line1, sizeof(line1), "CH1:");
            if (flowing) {
                snprintf(line1 + 4, sizeof(line1) - 4, " ALARM!   ");
            } else {
                snprintf(line1 + 4, sizeof(line1) - 4, "OK  %.1fL", rate);
            }
            snprintf(line2, sizeof(line2), "System RUNNING  ");
            break;
        }
            
        case STATE_ALARM: {
            snprintf(line1, sizeof(line1), "!! ALARM !!     ");
            
            // Get alarm channels (INVERTED: channel 1 if flowing)
            if (flow_sensor_is_flowing(flow_manager, 0)) {
                snprintf(line2, sizeof(line2), "CH1 (flow)      ");
            } else {
                snprintf(line2, sizeof(line2), "Unknown alarm   ");
            }
            break;
        }
            
        default:
            snprintf(line1, sizeof(line1), "ERROR STATE     ");
            snprintf(line2, sizeof(line2), "Restart system  ");
            break;
    }
    
    // Ensure strings are null-terminated
    line1[16] = '\0';
    line2[16] = '\0';
    
    lcd1602_print(lcd, line1, line2);
}