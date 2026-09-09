#include "lcd1602.h"
#include "config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <string.h>

static const char* TAG = "LCD1602";

// LCD commands
#define LCD_CLEAR 0x01
#define LCD_HOME 0x02
#define LCD_ENTRY_MODE 0x04
#define LCD_DISPLAY_CTRL 0x08
#define LCD_FUNCTION_SET 0x20

// Flags
#define LCD_ENTRY_INCREMENT 0x02
#define LCD_ENTRY_SHIFT 0x01
#define LCD_DISPLAY_ON 0x04
#define LCD_CURSOR_ON 0x02
#define LCD_BLINK_ON 0x01
#define LCD_8BIT_MODE 0x10
#define LCD_2LINE 0x08
#define LCD_5x8_DOTS 0x00

static void lcd_write_cmd(lcd1602_t* lcd, uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    esp_err_t ret = i2c_master_write_to_device(
        I2C_MASTER_NUM, 
        lcd->addr, 
        data, 
        2, 
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD command failed: %d", ret);
    }
    
    vTaskDelay(pdMS_TO_TICKS(5));
}

static void lcd_write_data(lcd1602_t* lcd, uint8_t data) {
    uint8_t buf[2] = {0x40, data};
    esp_err_t ret = i2c_master_write_to_device(
        I2C_MASTER_NUM, 
        lcd->addr, 
        buf, 
        2, 
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD data write failed: %d", ret);
    }
}

lcd1602_t* lcd1602_init(uint8_t addr, uint8_t cols, uint8_t rows) {
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
    
    // Initialize I2C - исправленная инициализация
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0;
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %d", ret);
        free(lcd);
        return NULL;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %d", ret);
        free(lcd);
        return NULL;
    }
    
    // Wait for LCD to power up
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Initialize LCD in 4-bit mode
    lcd_write_cmd(lcd, 0x33);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_cmd(lcd, 0x32);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_cmd(lcd, 0x28);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_cmd(lcd, 0x0C);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_cmd(lcd, 0x06);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_cmd(lcd, 0x01);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    lcd->initialized = true;
    ESP_LOGI(TAG, "LCD initialized at address 0x%02X", addr);
    
    return lcd;
}

void lcd1602_print(lcd1602_t* lcd, const char* line1, const char* line2) {
    if (!lcd || !lcd->initialized) return;
    
    lcd_write_cmd(lcd, LCD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));
    
    lcd_write_cmd(lcd, 0x80);
    if (line1) {
        for (int i = 0; line1[i] && i < lcd->cols; i++) {
            lcd_write_data(lcd, line1[i]);
        }
    }
    
    lcd_write_cmd(lcd, 0xC0);
    if (line2) {
        for (int i = 0; line2[i] && i < lcd->cols; i++) {
            lcd_write_data(lcd, line2[i]);
        }
    }
}

void lcd1602_clear(lcd1602_t* lcd) {
    if (!lcd || !lcd->initialized) return;
    lcd_write_cmd(lcd, LCD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd1602_set_backlight(lcd1602_t* lcd, bool on) {
    if (!lcd) return;
    lcd->backlight = on;
}

void lcd1602_deinit(lcd1602_t* lcd) {
    if (!lcd) return;
    i2c_driver_delete(I2C_MASTER_NUM);
    free(lcd);
}