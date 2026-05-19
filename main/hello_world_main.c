#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_wifi.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "LiquidCrystal_I2C.h"
#include "keypad.h"

#define TAG "SMART_LOCKER"

/* =========================
   WiFi / MQTT config
   ========================= */
#define WIFI_SSID               "Redmi K70"
#define WIFI_PASS               "baodang123"
#define MQTT_BROKER_URI         "mqtt://broker.hivemq.com"
#define DEVICE_ID               "locker_ctrl_01"

/* =========================
   LCD config
   ========================= */
#define LCD_ADDR                0x27
#define LCD_COLS                16
#define LCD_ROWS                2
#define LCD_SDA_IO              GPIO_NUM_21
#define LCD_SCL_IO              GPIO_NUM_22

/* =========================
   Relay pins
   ========================= */
#define RELAY_LOCKER1           GPIO_NUM_18
#define RELAY_LOCKER2           GPIO_NUM_19

/* =========================
   Keypad pins
   ========================= */
#define KEYPAD_R1               GPIO_NUM_32
#define KEYPAD_R2               GPIO_NUM_33
#define KEYPAD_R3               GPIO_NUM_25
#define KEYPAD_R4               GPIO_NUM_26
#define KEYPAD_C1               GPIO_NUM_27
#define KEYPAD_C2               GPIO_NUM_14
#define KEYPAD_C3               GPIO_NUM_16
#define KEYPAD_C4               GPIO_NUM_13

/* =========================
   App config
   ========================= */
#define DEFAULT_PIN_1           "1111"
#define DEFAULT_PIN_2           "2222"
#define MAX_PIN_LEN             8
#define RELAY_PULSE_MS          3000
#define HEARTBEAT_INTERVAL_MS   30000

typedef enum {
    STATE_SELECT_LOCKER = 0,
    STATE_ENTER_PIN,
    STATE_ACTION_MENU
} app_state_t;

static app_state_t g_state = STATE_SELECT_LOCKER;
static int g_selected_locker = 0;
static char g_input_pin[MAX_PIN_LEN + 1] = {0};
static int g_input_len = 0;

static char g_pin1[MAX_PIN_LEN + 1] = DEFAULT_PIN_1;
static char g_pin2[MAX_PIN_LEN + 1] = DEFAULT_PIN_2;

static esp_mqtt_client_handle_t g_mqtt_client = NULL;
static bool g_mqtt_started = false;

/* =========================
   Helper: Topics
   ========================= */
static void topic_cmd(char *buf, size_t len)
{
    snprintf(buf, len, "smartlocker/%s/cmd", DEVICE_ID);
}

static void topic_status(char *buf, size_t len)
{
    snprintf(buf, len, "smartlocker/%s/status", DEVICE_ID);
}

static void topic_event(char *buf, size_t len)
{
    snprintf(buf, len, "smartlocker/%s/event", DEVICE_ID);
}

static void topic_ack(char *buf, size_t len)
{
    snprintf(buf, len, "smartlocker/%s/ack", DEVICE_ID);
}

/* =========================
   NVS
   ========================= */
static void load_pins_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    size_t len1 = sizeof(g_pin1);
    err = nvs_get_str(nvs, "pin1", g_pin1, &len1);
    if (err != ESP_OK) {
        strlcpy(g_pin1, DEFAULT_PIN_1, sizeof(g_pin1));
        nvs_set_str(nvs, "pin1", g_pin1);
    }

    size_t len2 = sizeof(g_pin2);
    err = nvs_get_str(nvs, "pin2", g_pin2, &len2);
    if (err != ESP_OK) {
        strlcpy(g_pin2, DEFAULT_PIN_2, sizeof(g_pin2));
        nvs_set_str(nvs, "pin2", g_pin2);
    }

    nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "PIN1=%s, PIN2=%s", g_pin1, g_pin2);
}

static bool save_pin_to_nvs(int locker_id, const char *new_pin)
{
    nvs_handle_t nvs;
    if (nvs_open("storage", NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }

    esp_err_t err = ESP_FAIL;

    if (locker_id == 1) {
        err = nvs_set_str(nvs, "pin1", new_pin);
        if (err == ESP_OK) {
            strlcpy(g_pin1, new_pin, sizeof(g_pin1));
        }
    } else if (locker_id == 2) {
        err = nvs_set_str(nvs, "pin2", new_pin);
        if (err == ESP_OK) {
            strlcpy(g_pin2, new_pin, sizeof(g_pin2));
        }
    } else {
        nvs_close(nvs);
        return false;
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return (err == ESP_OK);
}

/* =========================
   LCD helpers
   ========================= */
static void lcd_show_select_locker(void)
{
    LCDI2C_clear();
    LCDI2C_setCursor(0, 0);
    LCDI2C_print("Chon tu:");
    LCDI2C_setCursor(0, 1);
    LCDI2C_print("1:T1   2:T2");
}

static void lcd_show_enter_pin(int locker)
{
    char line1[17];
    snprintf(line1, sizeof(line1), "T%d - Nhap MK:", locker);

    LCDI2C_clear();
    LCDI2C_setCursor(0, 0);
    LCDI2C_print(line1);
    LCDI2C_setCursor(0, 1);

    for (int i = 0; i < g_input_len; i++) {
        LCDI2C_write('*');
    }
}

static void lcd_show_message(const char *line1, const char *line2, int delay_ms)
{
    LCDI2C_clear();
    LCDI2C_setCursor(0, 0);
    LCDI2C_print(line1 ? line1 : "");
    LCDI2C_setCursor(0, 1);
    LCDI2C_print(line2 ? line2 : "");

    if (delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void lcd_show_action_menu(int locker)
{
    char line1[17];
    snprintf(line1, sizeof(line1), "Tu %d hop le", locker);

    LCDI2C_clear();
    LCDI2C_setCursor(0, 0);
    LCDI2C_print(line1);
    LCDI2C_setCursor(0, 1);
    LCDI2C_print("1:Mo 2:Dong");
}

/* =========================
   Relay control
   ========================= */
static void relay_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_LOCKER1) | (1ULL << RELAY_LOCKER2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_set_level(RELAY_LOCKER1, 0);
    gpio_set_level(RELAY_LOCKER2, 0);
}

static void open_locker(int locker_id)
{
    gpio_num_t relay_pin = (locker_id == 1) ? RELAY_LOCKER1 : RELAY_LOCKER2;

    char line1[17];
    snprintf(line1, sizeof(line1), "Dang mo Tu %d", locker_id);
    lcd_show_message(line1, "Xin cho...", 0);

    gpio_set_level(relay_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(RELAY_PULSE_MS));
    gpio_set_level(relay_pin, 0);
}

/* =========================
   MQTT publish helpers
   ========================= */
static void mqtt_publish_status(void)
{
    if (!g_mqtt_client) {
        return;
    }

    char topic[64];
    topic_status(topic, sizeof(topic));

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return;
    }

    cJSON_AddStringToObject(root, "deviceId", DEVICE_ID);
    cJSON_AddBoolToObject(root, "online", true);
    cJSON_AddBoolToObject(root, "locker1Ready", true);
    cJSON_AddBoolToObject(root, "locker2Ready", true);
    cJSON_AddNumberToObject(root, "state", g_state);

    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        esp_mqtt_client_publish(g_mqtt_client, topic, payload, 0, 1, 0);
        cJSON_free(payload);
    }

    cJSON_Delete(root);
}

static void mqtt_publish_event(int locker_id, const char *event_name, const char *method)
{
    if (!g_mqtt_client) {
        return;
    }

    char topic[64];
    topic_event(topic, sizeof(topic));

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return;
    }

    cJSON_AddStringToObject(root, "deviceId", DEVICE_ID);
    cJSON_AddNumberToObject(root, "lockerId", locker_id);
    cJSON_AddStringToObject(root, "event", event_name ? event_name : "");
    cJSON_AddStringToObject(root, "method", method ? method : "");

    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        esp_mqtt_client_publish(g_mqtt_client, topic, payload, 0, 1, 0);
        cJSON_free(payload);
    }

    cJSON_Delete(root);
}

static void mqtt_publish_ack(const char *request_id,
                             const char *cmd,
                             const char *result,
                             const char *message)
{
    if (!g_mqtt_client) {
        return;
    }

    char topic[64];
    topic_ack(topic, sizeof(topic));

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return;
    }

    cJSON_AddStringToObject(root, "deviceId", DEVICE_ID);
    cJSON_AddStringToObject(root, "requestId", request_id ? request_id : "");
    cJSON_AddStringToObject(root, "cmd", cmd ? cmd : "");
    cJSON_AddStringToObject(root, "result", result ? result : "");
    cJSON_AddStringToObject(root, "message", message ? message : "");

    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        esp_mqtt_client_publish(g_mqtt_client, topic, payload, 0, 1, 0);
        cJSON_free(payload);
    }

    cJSON_Delete(root);
}

/* =========================
   MQTT command handler
   ========================= */
static void handle_mqtt_command(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) {
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    cJSON *requestId = cJSON_GetObjectItem(root, "requestId");
    cJSON *lockerId = cJSON_GetObjectItem(root, "lockerId");
    cJSON *newPin = cJSON_GetObjectItem(root, "newPin");

    const char *cmd_str = (cJSON_IsString(cmd) && cmd->valuestring) ? cmd->valuestring : NULL;
    const char *req_str = (cJSON_IsString(requestId) && requestId->valuestring) ? requestId->valuestring : "";
    int locker = cJSON_IsNumber(lockerId) ? lockerId->valueint : 0;

    if (!cmd_str) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(cmd_str, "OPEN_LOCKER") == 0) {
        if (locker == 1 || locker == 2) {
            open_locker(locker);
            mqtt_publish_ack(req_str, "OPEN_LOCKER", "OK", "Locker opened");
            mqtt_publish_event(locker, "REMOTE_OPEN", "WEB");
        } else {
            mqtt_publish_ack(req_str, "OPEN_LOCKER", "FAIL", "Invalid lockerId");
        }
    } else if (strcmp(cmd_str, "SET_PIN") == 0) {
        if ((locker == 1 || locker == 2) &&
            cJSON_IsString(newPin) &&
            newPin->valuestring) {

            size_t pin_len = strlen(newPin->valuestring);
            if (pin_len > 0 && pin_len <= MAX_PIN_LEN) {
                if (save_pin_to_nvs(locker, newPin->valuestring)) {
                    mqtt_publish_ack(req_str, "SET_PIN", "OK", "PIN updated");
                    mqtt_publish_event(locker, "PIN_CHANGED", "WEB");
                } else {
                    mqtt_publish_ack(req_str, "SET_PIN", "FAIL", "Save PIN failed");
                }
            } else {
                mqtt_publish_ack(req_str, "SET_PIN", "FAIL", "Invalid PIN length");
            }
        } else {
            mqtt_publish_ack(req_str, "SET_PIN", "FAIL", "Invalid payload");
        }
    }

    cJSON_Delete(root);
}

/* =========================
   MQTT event
   ========================= */
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT connected");

            char topic[64];
            topic_cmd(topic, sizeof(topic));
            esp_mqtt_client_subscribe(g_mqtt_client, topic, 1);

            mqtt_publish_status();
            break;
        }

        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "MQTT data: topic=%.*s data=%.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);

            char cmd_topic[64];
            topic_cmd(cmd_topic, sizeof(cmd_topic));

            if ((int)strlen(cmd_topic) == event->topic_len &&
                strncmp(event->topic, cmd_topic, event->topic_len) == 0) {
                handle_mqtt_command(event->data, event->data_len);
            }
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        default:
            break;
    }
}

static void mqtt_app_start(void)
{
    if (g_mqtt_started) {
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!g_mqtt_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(g_mqtt_client);
    g_mqtt_started = true;
}

/* =========================
   WiFi
   ========================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        mqtt_app_start();
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* =========================
   App logic
   ========================= */
static void reset_to_select_state(void)
{
    g_state = STATE_SELECT_LOCKER;
    g_selected_locker = 0;
    memset(g_input_pin, 0, sizeof(g_input_pin));
    g_input_len = 0;
    lcd_show_select_locker();
}

static bool is_correct_pin(int locker, const char *pin)
{
    if (locker == 1) {
        return strcmp(pin, g_pin1) == 0;
    }
    if (locker == 2) {
        return strcmp(pin, g_pin2) == 0;
    }
    return false;
}

static void process_key(char key)
{
    if (key == 0) {
        return;
    }

    ESP_LOGI(TAG, "Key pressed: %c", key);

    switch (g_state) {
        case STATE_SELECT_LOCKER:
            if (key == '1') {
                g_selected_locker = 1;
                g_state = STATE_ENTER_PIN;
                g_input_len = 0;
                memset(g_input_pin, 0, sizeof(g_input_pin));
                lcd_show_enter_pin(g_selected_locker);
            } else if (key == '2') {
                g_selected_locker = 2;
                g_state = STATE_ENTER_PIN;
                g_input_len = 0;
                memset(g_input_pin, 0, sizeof(g_input_pin));
                lcd_show_enter_pin(g_selected_locker);
            }
            break;

        case STATE_ENTER_PIN:
            if (key >= '0' && key <= '9') {
                if (g_input_len < MAX_PIN_LEN) {
                    g_input_pin[g_input_len++] = key;
                    g_input_pin[g_input_len] = '\0';
                    lcd_show_enter_pin(g_selected_locker);
                }
            } else if (key == '*') {
                if (g_input_len > 0) {
                    g_input_len--;
                    g_input_pin[g_input_len] = '\0';
                    lcd_show_enter_pin(g_selected_locker);
                }
            } else if (key == 'C' || key == 'D') {
                reset_to_select_state();
            } else if (key == '#'|| key == 'A') {
                if (is_correct_pin(g_selected_locker, g_input_pin)) {
                    mqtt_publish_event(g_selected_locker, "PIN_SUCCESS", "KEYPAD");
                    g_state = STATE_ACTION_MENU;
                    lcd_show_action_menu(g_selected_locker);
                } else {
                    mqtt_publish_event(g_selected_locker, "PIN_FAIL", "KEYPAD");
                    lcd_show_message("Sai mat khau!", "Nhap lai...", 1200);
                    g_input_len = 0;
                    memset(g_input_pin, 0, sizeof(g_input_pin));
                    lcd_show_enter_pin(g_selected_locker);
                }
            }
            break;

        case STATE_ACTION_MENU:
            if (key == '1') {
                open_locker(g_selected_locker);
                mqtt_publish_event(g_selected_locker, "LOCAL_OPEN", "KEYPAD");
                lcd_show_action_menu(g_selected_locker);
            } else if (key == '2') {
                mqtt_publish_event(g_selected_locker, "CLOSE_ACTION", "KEYPAD");
                reset_to_select_state();
            } else if (key == 'C' || key == 'D') {
                reset_to_select_state();
            }
            break;

        default:
            reset_to_select_state();
            break;
    }
}

/* =========================
   Tasks
   ========================= */
static void keypad_task(void *arg)
{
    (void)arg;

    while (1) {
        char key = scanKeyPress();
        if (key) {
            process_key(key);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void heartbeat_task(void *arg)
{
    (void)arg;

    while (1) {
        mqtt_publish_status();
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

/* =========================
   app_main
   ========================= */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(LCDI2C_init(LCD_ADDR, LCD_COLS, LCD_ROWS, LCD_SDA_IO, LCD_SCL_IO));
    LCDI2C_backlight();

    relay_gpio_init();

    ESP_ERROR_CHECK(setKeypadPins(
        KEYPAD_R1, KEYPAD_R2, KEYPAD_R3, KEYPAD_R4,
        KEYPAD_C1, KEYPAD_C2, KEYPAD_C3, KEYPAD_C4
    ));

    load_pins_from_nvs();

    lcd_show_message("Smart Locker", "Khoi dong...", 1200);

    wifi_init_sta();

    reset_to_select_state();

    xTaskCreate(keypad_task, "keypad_task", 4096, NULL, 5, NULL);
    xTaskCreate(heartbeat_task, "heartbeat_task", 4096, NULL, 5, NULL);
}
