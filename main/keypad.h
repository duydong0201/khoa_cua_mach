#ifndef KEYPAD_H
#define KEYPAD_H

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Called by scanKeyPress() function only to output characters
// User can re-assign new values to output different chars.
char keyMap(char key);

// Returns characters as per the pressed key.
// Returned characters are mapped in keyMap() function.
// Returns '\0' if no key is pressed.
char scanKeyPress(void);

// Pin 1 - 4 of keypad module connects to row_1 - row_4
// Pin 5 - 8 of keypad module connects to col_1 - col_4
esp_err_t setKeypadPins(gpio_num_t row_1,
                        gpio_num_t row_2,
                        gpio_num_t row_3,
                        gpio_num_t row_4,
                        gpio_num_t col_1,
                        gpio_num_t col_2,
                        gpio_num_t col_3,
                        gpio_num_t col_4);

#ifdef __cplusplus
}
#endif

#endif
