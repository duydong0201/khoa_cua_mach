#ifndef LIQUIDCRYSTAL_I2C_H
#define LIQUIDCRYSTAL_I2C_H

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

// commands
#define LCD_CLEARDISPLAY            0x01
#define LCD_RETURNHOME              0x02
#define LCD_ENTRYMODESET            0x04
#define LCD_DISPLAYCONTROL          0x08
#define LCD_CURSORSHIFT             0x10
#define LCD_FUNCTIONSET             0x20
#define LCD_SETCGRAMADDR            0x40
#define LCD_SETDDRAMADDR            0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT              0x00
#define LCD_ENTRYLEFT               0x02
#define LCD_ENTRYSHIFTINCREMENT     0x01
#define LCD_ENTRYSHIFTDECREMENT     0x00

// flags for display on/off control
#define LCD_DISPLAYON               0x04
#define LCD_DISPLAYOFF              0x00
#define LCD_CURSORON                0x02
#define LCD_CURSOROFF               0x00
#define LCD_BLINKON                 0x01
#define LCD_BLINKOFF                0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE             0x08
#define LCD_CURSORMOVE              0x00
#define LCD_MOVERIGHT               0x04
#define LCD_MOVELEFT                0x00

// flags for function set
#define LCD_8BITMODE                0x10
#define LCD_4BITMODE                0x00
#define LCD_2LINE                   0x08
#define LCD_1LINE                   0x00
#define LCD_5x10DOTS                0x04
#define LCD_5x8DOTS                 0x00

// flags for backlight control
#define LCD_BACKLIGHT               0x08
#define LCD_NOBACKLIGHT             0x00

// PCF8574 pin bit mapping
#define En                          0x04
#define Rw                          0x02
#define Rs                          0x01

typedef struct
{
    uint8_t addr;
    uint8_t displayfunction;
    uint8_t displaycontrol;
    uint8_t displaymode;
    uint8_t cols;
    uint8_t rows;
    uint8_t backlightval;

    i2c_port_t i2c_port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t clk_speed_hz;
} LiquidCrystal_I2C_Def;

// Public API
esp_err_t LCDI2C_init(uint8_t lcd_addr,
                      uint8_t lcd_cols,
                      uint8_t lcd_rows,
                      gpio_num_t sda_pin,
                      gpio_num_t scl_pin);

esp_err_t LCDI2C_begin(void);
void LCDI2C_clear(void);
void LCDI2C_home(void);
void LCDI2C_noDisplay(void);
void LCDI2C_display(void);
void LCDI2C_noBlink(void);
void LCDI2C_blink(void);
void LCDI2C_noCursor(void);
void LCDI2C_cursor(void);
void LCDI2C_scrollDisplayLeft(void);
void LCDI2C_scrollDisplayRight(void);
void LCDI2C_printLeft(void);
void LCDI2C_printRight(void);
void LCDI2C_leftToRight(void);
void LCDI2C_rightToLeft(void);
void LCDI2C_shiftIncrement(void);
void LCDI2C_shiftDecrement(void);
void LCDI2C_noBacklight(void);
void LCDI2C_backlight(void);
void LCDI2C_autoscroll(void);
void LCDI2C_noAutoscroll(void);
void LCDI2C_createChar(uint8_t location, uint8_t charmap[]);
void LCDI2C_setCursor(uint8_t col, uint8_t row);
void LCDI2C_command(uint8_t value);
void LCDI2C_write(uint8_t value);
void LCDI2C_print(const char *str);

// compatibility aliases
void LCDI2C_blink_on(void);
void LCDI2C_blink_off(void);
void LCDI2C_cursor_on(void);
void LCDI2C_cursor_off(void);
void LCDI2C_setBacklight(uint8_t new_val);
void LCDI2C_load_custom_character(uint8_t char_num, uint8_t *rows);
void LCDI2C_printstr(const char str[]);

// low level
void LCDI2C_send(uint8_t value, uint8_t mode);
void LCDI2C_write4bits(uint8_t value);
void LCDI2C_expanderWrite(uint8_t data);
void LCDI2C_pulseEnable(uint8_t data);

#endif
