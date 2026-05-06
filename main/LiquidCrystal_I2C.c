#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "LiquidCrystal_I2C.h"

#define LCD_I2C_PORT            I2C_NUM_1
#define LCD_I2C_FREQ_HZ         100000
#define LCD_I2C_TIMEOUT_MS      1000

static const char *TAG = "LCD_I2C";
static LiquidCrystal_I2C_Def lcdi2c;

static esp_err_t lcd_i2c_master_init(gpio_num_t sda_pin, gpio_num_t scl_pin)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = LCD_I2C_FREQ_HZ,
#if ESP_IDF_VERSION_MAJOR >= 5
        .clk_flags = 0,
#endif
    };

    esp_err_t err = i2c_param_config(LCD_I2C_PORT, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(LCD_I2C_PORT, conf.mode, 0, 0, 0);
    if (err == ESP_ERR_INVALID_STATE) {
        // Driver already installed
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t LCDI2C_init(uint8_t lcd_addr,
                      uint8_t lcd_cols,
                      uint8_t lcd_rows,
                      gpio_num_t sda_pin,
                      gpio_num_t scl_pin)
{
    memset(&lcdi2c, 0, sizeof(lcdi2c));

    lcdi2c.addr = lcd_addr;
    lcdi2c.cols = lcd_cols;
    lcdi2c.rows = lcd_rows;
    lcdi2c.backlightval = LCD_BACKLIGHT;
    lcdi2c.i2c_port = LCD_I2C_PORT;
    lcdi2c.sda_pin = sda_pin;
    lcdi2c.scl_pin = scl_pin;
    lcdi2c.clk_speed_hz = LCD_I2C_FREQ_HZ;

    esp_err_t err = lcd_i2c_master_init(sda_pin, scl_pin);
    if (err != ESP_OK) {
        return err;
    }

    return LCDI2C_begin();
}

esp_err_t LCDI2C_begin(void)
{
    lcdi2c.displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;

    if (lcdi2c.rows > 1) {
        lcdi2c.displayfunction |= LCD_2LINE;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    LCDI2C_expanderWrite(lcdi2c.backlightval);
    vTaskDelay(pdMS_TO_TICKS(1000));

    LCDI2C_write4bits(0x03 << 4);
    esp_rom_delay_us(4500);

    LCDI2C_write4bits(0x03 << 4);
    esp_rom_delay_us(4500);

    LCDI2C_write4bits(0x03 << 4);
    esp_rom_delay_us(150);

    LCDI2C_write4bits(0x02 << 4);

    LCDI2C_command(LCD_FUNCTIONSET | lcdi2c.displayfunction);

    lcdi2c.displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    LCDI2C_display();

    LCDI2C_clear();

    lcdi2c.displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    LCDI2C_command(LCD_ENTRYMODESET | lcdi2c.displaymode);

    LCDI2C_home();

    return ESP_OK;
}

void LCDI2C_clear(void)
{
    LCDI2C_command(LCD_CLEARDISPLAY);
    esp_rom_delay_us(2000);
}

void LCDI2C_home(void)
{
    LCDI2C_command(LCD_RETURNHOME);
    esp_rom_delay_us(2000);
}

void LCDI2C_setCursor(uint8_t col, uint8_t row)
{
    static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};

    if (row >= lcdi2c.rows) {
        row = lcdi2c.rows - 1;
    }

    LCDI2C_command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void LCDI2C_noDisplay(void)
{
    lcdi2c.displaycontrol &= ~LCD_DISPLAYON;
    LCDI2C_command(LCD_DISPLAYCONTROL | lcdi2c.displaycontrol);
}

void LCDI2C_display(void)
{
    lcdi2c.displaycontrol |= LCD_DISPLAYON;
    LCDI2C_command(LCD_DISPLAYCONTROL | lcdi2c.displaycontrol);
}

void LCDI2C_noCursor(void)
{
    lcdi2c.displaycontrol &= ~LCD_CURSORON;
    LCDI2C_command(LCD_DISPLAYCONTROL | lcdi2c.displaycontrol);
}

void LCDI2C_cursor(void)
{
    lcdi2c.displaycontrol |= LCD_CURSORON;
    LCDI2C_command(LCD_DISPLAYCONTROL | lcdi2c.displaycontrol);
}

void LCDI2C_noBlink(void)
{
    lcdi2c.displaycontrol &= ~LCD_BLINKON;
    LCDI2C_command(LCD_DISPLAYCONTROL | lcdi2c.displaycontrol);
}

void LCDI2C_blink(void)
{
    lcdi2c.displaycontrol |= LCD_BLINKON;
    LCDI2C_command(LCD_DISPLAYCONTROL | lcdi2c.displaycontrol);
}

void LCDI2C_scrollDisplayLeft(void)
{
    LCDI2C_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void LCDI2C_scrollDisplayRight(void)
{
    LCDI2C_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void LCDI2C_leftToRight(void)
{
    lcdi2c.displaymode |= LCD_ENTRYLEFT;
    LCDI2C_command(LCD_ENTRYMODESET | lcdi2c.displaymode);
}

void LCDI2C_rightToLeft(void)
{
    lcdi2c.displaymode &= ~LCD_ENTRYLEFT;
    LCDI2C_command(LCD_ENTRYMODESET | lcdi2c.displaymode);
}

void LCDI2C_autoscroll(void)
{
    lcdi2c.displaymode |= LCD_ENTRYSHIFTINCREMENT;
    LCDI2C_command(LCD_ENTRYMODESET | lcdi2c.displaymode);
}

void LCDI2C_noAutoscroll(void)
{
    lcdi2c.displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    LCDI2C_command(LCD_ENTRYMODESET | lcdi2c.displaymode);
}

void LCDI2C_createChar(uint8_t location, uint8_t charmap[])
{
    location &= 0x07;
    LCDI2C_command(LCD_SETCGRAMADDR | (location << 3));

    for (int i = 0; i < 8; i++) {
        LCDI2C_write(charmap[i]);
    }
}

void LCDI2C_noBacklight(void)
{
    lcdi2c.backlightval = LCD_NOBACKLIGHT;
    LCDI2C_expanderWrite(0);
}

void LCDI2C_backlight(void)
{
    lcdi2c.backlightval = LCD_BACKLIGHT;
    LCDI2C_expanderWrite(0);
}

void LCDI2C_command(uint8_t value)
{
    LCDI2C_send(value, 0);
}

void LCDI2C_write(uint8_t value)
{
    LCDI2C_send(value, Rs);
}

void LCDI2C_send(uint8_t value, uint8_t mode)
{
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = (value << 4) & 0xF0;

    LCDI2C_write4bits(highnib | mode);
    LCDI2C_write4bits(lownib | mode);
}

void LCDI2C_write4bits(uint8_t value)
{
    LCDI2C_expanderWrite(value);
    LCDI2C_pulseEnable(value);
}

void LCDI2C_expanderWrite(uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcdi2c.addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data | lcdi2c.backlightval, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(lcdi2c.i2c_port,
                                         cmd,
                                         pdMS_TO_TICKS(LCD_I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C write failed: %s", esp_err_to_name(err));
    }

    i2c_cmd_link_delete(cmd);
}

void LCDI2C_pulseEnable(uint8_t data)
{
    LCDI2C_expanderWrite(data | En);
    esp_rom_delay_us(1);

    LCDI2C_expanderWrite(data & ~En);
    esp_rom_delay_us(50);
}

void LCDI2C_print(const char *str)
{
    if (str == NULL) {
        return;
    }

    while (*str) {
        LCDI2C_write((uint8_t)*str++);
    }
}

// Optional compatibility stubs
void LCDI2C_cursor_on(void)
{
    LCDI2C_cursor();
}

void LCDI2C_cursor_off(void)
{
    LCDI2C_noCursor();
}

void LCDI2C_blink_on(void)
{
    LCDI2C_blink();
}

void LCDI2C_blink_off(void)
{
    LCDI2C_noBlink();
}

void LCDI2C_load_custom_character(uint8_t char_num, uint8_t *rows)
{
    LCDI2C_createChar(char_num, rows);
}

void LCDI2C_setBacklight(uint8_t new_val)
{
    if (new_val) {
        LCDI2C_backlight();
    } else {
        LCDI2C_noBacklight();
    }
}

void LCDI2C_printstr(const char str[])
{
    LCDI2C_print(str);
}

// Placeholders to keep compatibility with some old APIs
void LCDI2C_printLeft(void)
{
    LCDI2C_leftToRight();
}

void LCDI2C_printRight(void)
{
    LCDI2C_rightToLeft();
}

void LCDI2C_shiftIncrement(void)
{
    lcdi2c.displaymode |= LCD_ENTRYSHIFTINCREMENT;
    LCDI2C_command(LCD_ENTRYMODESET | lcdi2c.displaymode);
}

void LCDI2C_shiftDecrement(void)
{
    lcdi2c.displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    LCDI2C_command(LCD_ENTRYMODESET | lcdi2c.displaymode);
}
