#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "esp_system.h"
#include "driver/gpio.h"
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
#define I2C_MASTER_NUM          0
#define I2C_MASTER_SDA_IO       GPIO_NUM_6
#define I2C_MASTER_SCL_IO       GPIO_NUM_7
#define I2C_MASTER_FREQ_HZ      50000
#define I2C_TIMEOUT_MS          2000
#define I2C_RETRY_COUNT         5

//=============================================================================
// I2C Device Addresses
//=============================================================================
#define LCD_ADDRESS             0x27
#define RELAY_EXPANDER_ADDR     0x23
#define BUTTON_EXPANDER_ADDR    0x25

//=============================================================================
// Relay Configuration (PCF8575 at 0x23)
//=============================================================================
#define RELAY_MAIN              0
#define RELAY_READY             1
#define RELAY_BEACON            2
#define RELAY_POWER_LAMP        3
#define RELAY_MAX               16

#define RELAY_HOLD_TIME_MS      300
#define RELAY_DEBOUNCE_DELAY_MS 100

//=============================================================================
// Button Configuration (PCF8575 at 0x25)
//=============================================================================
#define BUTTON_START            0
#define BUTTON_STOP             1
#define BUTTON_RESET            2
#define BUTTON_MAX              16

//=============================================================================
// Flow Sensors Configuration - 6 датчиков
//=============================================================================
#define FLOW_SENSORS_COUNT      6

// Безопасные пины для ESP32-C6 (не bootstrap, не конфликтуют с Flash/WiFi)
#define FLOW_SENSOR_1           GPIO_NUM_10
#define FLOW_SENSOR_2           GPIO_NUM_11
#define FLOW_SENSOR_3           GPIO_NUM_20
#define FLOW_SENSOR_4           GPIO_NUM_21
#define FLOW_SENSOR_5           GPIO_NUM_22
#define FLOW_SENSOR_6           GPIO_NUM_23

// Коэффициент для ZJ-B1 (1/2") - для справки
#define FLOW_PULSES_PER_LITER   450.0f

// Таймаут потери потока - 1 секунда (критично для индукторов)
#define FLOW_TIMEOUT_MS         1000

#define FLOW_CALC_INTERVAL_MS   1000

//=============================================================================
// Display Configuration
//=============================================================================
#define LCD_COLS                16
#define LCD_ROWS                2
#define LCD_INIT_DELAY_MS       200
#define LCD_CMD_DELAY_MS        10

//=============================================================================
// System Parameters
//=============================================================================
#define DEBOUNCE_DELAY_MS       50
#define DISPLAY_UPDATE_MS       500
#define SYSTEM_START_DELAY_MS   100
#define ALARM_HOLD_TIME_MS      3000

//=============================================================================
// Watchdog Configuration
//=============================================================================
#define WATCHDOG_TIMEOUT_SEC    10
#define WATCHDOG_FEED_INTERVAL_MS 1000

//=============================================================================
// Task Configuration
//=============================================================================
#define TASK_PRIORITY_CONTROL   5
#define TASK_PRIORITY_BUTTONS   4
#define TASK_PRIORITY_FLOW      3
#define TASK_PRIORITY_DISPLAY   2
#define TASK_PRIORITY_WATCHDOG  1

#define TASK_STACK_CONTROL      8192
#define TASK_STACK_BUTTONS      4096
#define TASK_STACK_FLOW         4096
#define TASK_STACK_DISPLAY      4096
#define TASK_STACK_WATCHDOG     2048

//=============================================================================
// Debug
//=============================================================================
#define TEST_MODE_ENABLED       0

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H