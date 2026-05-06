#include "keypad.h"

#include <stdio.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

static const char *TAG = "KEYPAD";

// Key debounce time in ms
static const long timeOffset = 300;

static gpio_num_t rowPin_1, rowPin_2, rowPin_3, rowPin_4;
static gpio_num_t colPin_1, colPin_2, colPin_3, colPin_4;

// Called by scanKeyPress() function only to output characters
// User can re-assign new values to output different chars. e.g. Returning '$' when pressed 'A'
char keyMap(char key)
{
    char val = '\0';

    switch (key)
    {
        case '1':
            val = '1';
            break;
        case '2':
            val = '2';
            break;
        case '3':
            val = '3';
            break;
        case 'A':
            val = 'A';
            break;

        case '4':
            val = '4';
            break;
        case '5':
            val = '5';
            break;
        case '6':
            val = '6';
            break;
        case 'B':
            val = 'B';
            break;

        case '7':
            val = '7';
            break;
        case '8':
            val = '8';
            break;
        case '9':
            val = '9';
            break;
        case 'C':
            val = 'C';
            break;

        case '*':
            val = '*';   // nếu muốn giống code Arduino mẫu thì đổi thành '.'
            break;
        case '0':
            val = '0';
            break;
        case '#':
            val = '#';
            break;
        case 'D':
            val = 'D';
            break;

        default:
            ESP_LOGW(TAG, "Unmatched key");
            break;
    }

    return val;
}

// Returns characters as per the pressed key.
// Returned characters are mapped in keyMap() function
// Returns '\0' if no key is pressed
char scanKeyPress(void)
{
    char pressedKey = '\0';

    static int64_t now_ms = 0;
    int64_t current_ms = esp_timer_get_time() / 1000;
    int64_t repeatTime = current_ms - now_ms;

    // *********************************   ROW 1   ******************************************** //

    gpio_set_level(rowPin_1, 0);
    esp_rom_delay_us(5);

    if (!gpio_get_level(colPin_1) && repeatTime > timeOffset)
    {
        pressedKey = '1';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_2) && repeatTime > timeOffset)
    {
        pressedKey = '2';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_3) && repeatTime > timeOffset)
    {
        pressedKey = '3';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_4) && repeatTime > timeOffset)
    {
        pressedKey = 'A';
        now_ms = current_ms;
    }
    else
    {
        pressedKey = '\0';
    }

    gpio_set_level(rowPin_1, 1);

    if (pressedKey != '\0')
        return keyMap(pressedKey);

    // *********************************   ROW 2   ******************************************** //

    gpio_set_level(rowPin_2, 0);
    esp_rom_delay_us(5);

    if (!gpio_get_level(colPin_1) && repeatTime > timeOffset)
    {
        pressedKey = '4';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_2) && repeatTime > timeOffset)
    {
        pressedKey = '5';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_3) && repeatTime > timeOffset)
    {
        pressedKey = '6';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_4) && repeatTime > timeOffset)
    {
        pressedKey = 'B';
        now_ms = current_ms;
    }
    else
    {
        pressedKey = '\0';
    }

    gpio_set_level(rowPin_2, 1);

    if (pressedKey != '\0')
        return keyMap(pressedKey);

    // *********************************   ROW 3   ******************************************** //

    gpio_set_level(rowPin_3, 0);
    esp_rom_delay_us(5);

    if (!gpio_get_level(colPin_1) && repeatTime > timeOffset)
    {
        pressedKey = '7';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_2) && repeatTime > timeOffset)
    {
        pressedKey = '8';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_3) && repeatTime > timeOffset)
    {
        pressedKey = '9';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_4) && repeatTime > timeOffset)
    {
        pressedKey = 'C';
        now_ms = current_ms;
    }
    else
    {
        pressedKey = '\0';
    }

    gpio_set_level(rowPin_3, 1);

    if (pressedKey != '\0')
        return keyMap(pressedKey);

    // *********************************   ROW 4   ******************************************** //

    gpio_set_level(rowPin_4, 0);
    esp_rom_delay_us(5);

    if (!gpio_get_level(colPin_1) && repeatTime > timeOffset)
    {
        pressedKey = '*';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_2) && repeatTime > timeOffset)
    {
        pressedKey = '0';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_3) && repeatTime > timeOffset)
    {
        pressedKey = '#';
        now_ms = current_ms;
    }
    else if (!gpio_get_level(colPin_4) && repeatTime > timeOffset)
    {
        pressedKey = 'D';
        now_ms = current_ms;
    }
    else
    {
        pressedKey = '\0';
    }

    gpio_set_level(rowPin_4, 1);

    if (pressedKey != '\0')
        return keyMap(pressedKey);

    return pressedKey;
}

// Pin 1 - 4 of keypad module connects to pin row_1 - row_4
// Pin 5 - 8 of keypad module connects to pin col_1 - col_4
esp_err_t setKeypadPins(gpio_num_t row_1,
                        gpio_num_t row_2,
                        gpio_num_t row_3,
                        gpio_num_t row_4,
                        gpio_num_t col_1,
                        gpio_num_t col_2,
                        gpio_num_t col_3,
                        gpio_num_t col_4)
{
    rowPin_1 = row_1;
    rowPin_2 = row_2;
    rowPin_3 = row_3;
    rowPin_4 = row_4;

    colPin_1 = col_1;
    colPin_2 = col_2;
    colPin_3 = col_3;
    colPin_4 = col_4;

    gpio_config_t row_conf = {
        .pin_bit_mask = (1ULL << rowPin_1) |
                        (1ULL << rowPin_2) |
                        (1ULL << rowPin_3) |
                        (1ULL << rowPin_4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&row_conf));

    gpio_set_level(rowPin_1, 1);
    gpio_set_level(rowPin_2, 1);
    gpio_set_level(rowPin_3, 1);
    gpio_set_level(rowPin_4, 1);

    gpio_config_t col_conf = {
        .pin_bit_mask = (1ULL << colPin_1) |
                        (1ULL << colPin_2) |
                        (1ULL << colPin_3) |
                        (1ULL << colPin_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&col_conf));

    return ESP_OK;
}
