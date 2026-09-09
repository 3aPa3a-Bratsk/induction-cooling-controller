#include "lcd1602.h"
#include "config.h"
#include "i2c_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include <string.h>

static const char* TAG = "LCD1602";

// LCD commands
#define LCD_CLEAR               0x01
#define LCD_HOME                0x02

static i2c_manager_t* g_i2c_manager = NULL;
static bool lcd_ready = false;

//=============================================================================
// Базовые функции PCF8574
//=============================================================================

static void lcd_write_pcf8574(uint8_t data) {
    if (!g_i2c_manager || !g_i2c_manager->initialized) return;
    data |= 0x08; // Backlight ON
    i2c_write_byte(g_i2c_manager->lcd_dev, data);
}

static void lcd_pulse_enable(uint8_t data) {
    lcd_write_pcf8574(data | 0x04);
    esp_rom_delay_us(2);
    lcd_write_pcf8574(data & ~0x04);
    esp_rom_delay_us(50);
}

static void lcd_send_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0);
    if (mode) data |= 0x01; // RS = 1 for data
    lcd_write_pcf8574(data);
    lcd_pulse_enable(data);
}

static void lcd_send_byte(uint8_t data, uint8_t mode) {
    lcd_send_nibble(data, mode);
    lcd_send_nibble(data << 4, mode);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_cmd(uint8_t cmd) {
    if (!lcd_ready) return;
    lcd_send_byte(cmd, 0);
    if (cmd == LCD_CLEAR || cmd == LCD_HOME) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void lcd_data(uint8_t data) {
    if (!lcd_ready) return;
    lcd_send_byte(data, 1);
}

static void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t address = (row == 0) ? 0x80 + col : 0xC0 + col;
    lcd_cmd(address);
}

//=============================================================================
// Инициализация
//=============================================================================

lcd1602_t* lcd1602_init(uint8_t addr, uint8_t cols, uint8_t rows, i2c_manager_t* i2c) {
    if (!i2c || !i2c->initialized) {
        ESP_LOGE(TAG, "I2C not initialized");
        return NULL;
    }
    
    g_i2c_manager = i2c;
    lcd_ready = false;
    
    lcd1602_t* lcd = (lcd1602_t*)malloc(sizeof(lcd1602_t));
    if (!lcd) {
        ESP_LOGE(TAG, "Failed to allocate LCD memory");
        return NULL;
    }
    
    lcd->addr = addr;
    lcd->cols = cols;
    lcd->rows = rows;
    lcd->initialized = false;
    lcd->backlight = true;
    
    ESP_LOGI(TAG, "Init LCD at 0x%02X", addr);
    
    vTaskDelay(pdMS_TO_TICKS(200));
    lcd_write_pcf8574(0x08);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Инициализация HD44780
    for (int i = 0; i < 3; i++) {
        lcd_send_nibble(0x30, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lcd_send_nibble(0x20, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    lcd_send_byte(0x28, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_byte(0x08, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    lcd_send_byte(LCD_CLEAR, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    
    lcd_send_byte(0x06, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_byte(0x0C, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_byte(LCD_HOME, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    lcd->initialized = true;
    lcd_ready = true;
    
    // Принудительная очистка экрана пробелами (все 16 позиций в каждой строке)
    for (int row = 0; row < 2; row++) {
        lcd_set_cursor(0, row);
        for (int col = 0; col < 16; col++) {
            lcd_data(' ');
        }
    }
    
    ESP_LOGI(TAG, "LCD ready");
    
    return lcd;
}

//=============================================================================
// Вывод строк с ПРИНУДИТЕЛЬНЫМ заполнением пробелами
//=============================================================================

void lcd1602_print(lcd1602_t* lcd, const char* line1, const char* line2) {
    if (!lcd || !lcd->initialized || !lcd_ready) return;
    
    // Строка 1
    lcd_set_cursor(0, 0);
    if (line1) {
        for (int i = 0; i < 16; i++) {
            if (line1[i] != '\0' && line1[i] != '\r' && line1[i] != '\n') {
                lcd_data(line1[i]);
            } else {
                lcd_data(' '); // Пробел, если строка закончилась
            }
        }
    } else {
        // Если строка NULL - заполняем пробелами
        for (int i = 0; i < 16; i++) {
            lcd_data(' ');
        }
    }
    
    // Строка 2
    lcd_set_cursor(0, 1);
    if (line2) {
        for (int i = 0; i < 16; i++) {
            if (line2[i] != '\0' && line2[i] != '\r' && line2[i] != '\n') {
                lcd_data(line2[i]);
            } else {
                lcd_data(' '); // Пробел, если строка закончилась
            }
        }
    } else {
        // Если строка NULL - заполняем пробелами
        for (int i = 0; i < 16; i++) {
            lcd_data(' ');
        }
    }
}

void lcd1602_clear(lcd1602_t* lcd) {
    if (!lcd || !lcd->initialized || !lcd_ready) return;
    lcd_cmd(LCD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(5));
}

void lcd1602_set_backlight(lcd1602_t* lcd, bool on) {
    if (!lcd) return;
    lcd->backlight = on;
    lcd_write_pcf8574(on ? 0x08 : 0x00);
}

void lcd1602_deinit(lcd1602_t* lcd) {
    if (!lcd) return;
    free(lcd);
}