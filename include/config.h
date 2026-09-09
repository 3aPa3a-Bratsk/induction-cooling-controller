#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "esp_system.h"
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// System Identification
//=============================================================================
#define SYSTEM_NAME "Induction Furnace Cooling Controller"
#define SYSTEM_MODEL "IFCC-ESP32C6-001"
#define SYSTEM_VERSION FIRMWARE_VERSION
#define SYSTEM_VENDOR "Industrial Automation Systems"

//=============================================================================
// I2C Configuration
//=============================================================================
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_SDA_IO       GPIO_NUM_6
#define I2C_MASTER_SCL_IO       GPIO_NUM_7
#define I2C_MASTER_FREQ_HZ      100000
#define I2C_TIMEOUT_MS          1000
#define I2C_RETRY_COUNT         3

//=============================================================================
// I2C Device Addresses
//=============================================================================
#define LCD_ADDRESS             0x27
#define RELAY_EXPANDER_ADDR     0x23    // 35 decimal
#define BUTTON_EXPANDER_ADDR    0x25    // 37 decimal

//=============================================================================
// Relay Configuration (PCF8575 at 0x23)
//=============================================================================
#define RELAY_MAIN              0       // P00 - Main contactor
#define RELAY_READY             1       // P01 - Ready/Work indication
#define RELAY_BEACON            2       // P02 - Beacon 12V
#define RELAY_POWER_LAMP        3       // P03 - Power lamp
#define RELAY_RESERVED_4        4       // P04 - Reserved for future
#define RELAY_RESERVED_5        5       // P05 - Reserved for future
#define RELAY_MAX               16

//=============================================================================
// Button Configuration (PCF8575 at 0x25)
//=============================================================================
#define BUTTON_START            0       // P00 - Start
#define BUTTON_STOP             1       // P01 - Stop
#define BUTTON_RESET            2       // P02 - Reset alarm
#define BUTTON_EMERGENCY        3       // P03 - Emergency stop (future)
#define BUTTON_MAX              16

//=============================================================================
// Flow Sensors Configuration
//=============================================================================
// TEST MODE: Single sensor on pin 20
#define FLOW_SENSOR_1           GPIO_NUM_20
// Production: All sensors
// #define FLOW_SENSOR_2        GPIO_NUM_9
// #define FLOW_SENSOR_3        GPIO_NUM_10
// #define FLOW_SENSOR_4        GPIO_NUM_11
// #define FLOW_SENSOR_5        GPIO_NUM_12
// #define FLOW_SENSOR_6        GPIO_NUM_13

#define FLOW_SENSORS_MAX        6
#define FLOW_SENSORS_ACTIVE     1   // Currently 1 sensor in test mode

// Flow sensor calibration
#define FLOW_PULSES_PER_LITER   7.5f    // Pulses per liter (sensor specific)
#define FLOW_TIMEOUT_MS         3000    // 3 seconds no pulses = no flow
#define FLOW_CALC_INTERVAL_MS   1000    // Calculate flow rate every second
#define FLOW_DEBOUNCE_MS        50      // Debounce for flow pulses

//=============================================================================
// Display Configuration (LCD1602)
//=============================================================================
#define LCD_COLS                16
#define LCD_ROWS                2
#define LCD_BACKLIGHT_ON        1
#define LCD_BACKLIGHT_OFF       0

//=============================================================================
// System Parameters
//=============================================================================
#define DEBOUNCE_DELAY_MS       50
#define DISPLAY_UPDATE_MS       200
#define SYSTEM_START_DELAY_MS   100     // Delay after start command
#define ALARM_HOLD_TIME_MS      3000    // Hold alarm state minimum time

//=============================================================================
// Watchdog Configuration
//=============================================================================
#define WATCHDOG_TIMEOUT_SEC    5
#define WATCHDOG_FEED_INTERVAL_MS 1000

//=============================================================================
// Task Configuration
//=============================================================================
#define TASK_PRIORITY_CONTROL   5       // Highest priority
#define TASK_PRIORITY_BUTTONS   4
#define TASK_PRIORITY_FLOW      3
#define TASK_PRIORITY_DISPLAY   2
#define TASK_PRIORITY_WATCHDOG  1       // Lowest priority

#define TASK_STACK_CONTROL      8192
#define TASK_STACK_BUTTONS      4096
#define TASK_STACK_FLOW         4096
#define TASK_STACK_DISPLAY      4096
#define TASK_STACK_WATCHDOG     2048

//=============================================================================
// Queue Configuration
//=============================================================================
#define EVENT_QUEUE_SIZE        32
#define COMMAND_QUEUE_SIZE      16
#define DATA_QUEUE_SIZE         16

//=============================================================================
// Logging Configuration
//=============================================================================
#define LOG_LEVEL_INFO          1
#define LOG_LEVEL_WARN          2
#define LOG_LEVEL_ERROR         3
#define LOG_LEVEL_DEBUG         4
#define LOG_LEVEL_VERBOSE       5

#define LOG_LEVEL_DEFAULT       LOG_LEVEL_INFO

//=============================================================================
// Safety and Certification
//=============================================================================
#define SAFETY_CHECK_ENABLED    1
#define ALARM_LATCH_ENABLED     1
#define EMERGENCY_STOP_ENABLED  1
#define CRITICAL_SECTION_TIMEOUT_MS 100

//=============================================================================
// Debug and Testing
//=============================================================================
#define TEST_MODE_ENABLED       1       // Test mode with inverted logic
#define SIMULATION_MODE         0

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H