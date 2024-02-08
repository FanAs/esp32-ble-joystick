/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "driver/gpio.h"
#include "hid_dev.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/timers.h"
#include "esp_adc/adc_continuous.h"
#include "bluetooth.h"
#include "continuous_adc.c"
#include "joystick.h"

#define MAIN_TAG "Main"

#define GPIO_SW_BUTTON GPIO_NUM_0

void deep_sleep(){
    ESP_LOGI("deep_sleep", "Going to sleep");

    disable_bluetooth();
    esp_deep_sleep_start();
}

#define LINEAR_GROWTH 0.08
#define INACTIVE_TIME_TO_SLEEP_MKS 60 * 1000 * 1000

void hid_demo_task(void *pvParameters)
{
    start_adc((adc_channel_t[2]){ADC_CHANNEL_1, ADC_CHANNEL_3}, 2);

    int8_t x = 0;
    int8_t y = 0;
    
    int64_t time = esp_timer_get_time();
    int64_t movement_start_x = 0;
    int64_t movement_start_y = 0;
    int64_t last_action = time;

    uint32_t chan_num;
    uint32_t data;
    uint16_t hid_conn_id;

    bool prev = gpio_get_level(GPIO_SW_BUTTON);
    while (1) {
        if (esp_timer_get_time() - INACTIVE_TIME_TO_SLEEP_MKS > last_action) {
            deep_sleep();
            break;
        }

        hid_conn_id = get_bluetooth_hid_connection_id();

        vTaskDelay(20 / portTICK_PERIOD_MS);

        while (adc_read_next(&chan_num, &data) == OK) {
            if (chan_num == 3) {
                x = joystick_tick(&movement_start_x, data);
            } else {
                y = joystick_tick(&movement_start_y, data);
            }
        }

        if (adc_read_next_err() != ESP_OK) {
            continue;
        }
        
        int level = gpio_get_level(GPIO_SW_BUTTON);

        if (x != 0 || y != 0 || level != prev) {
            last_action = esp_timer_get_time();
            if (level == 0) {
                esp_hidd_send_mouse_value(hid_conn_id, 0x01, x, y);
                vTaskDelay(10 / portTICK_PERIOD_MS);
            } else {
                esp_hidd_send_mouse_value(hid_conn_id, 0x00, x, y);
            }

            prev = level;
        } else {
            joystick_end(&movement_start_x);
            joystick_end(&movement_start_y);
        }
    }
}

void init_nvs() {
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
}

void configure_gpios() {
    ESP_ERROR_CHECK(gpio_config(&(gpio_config_t){
        .pin_bit_mask = (1ULL << GPIO_SW_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = true,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    }));
    
    gpio_deep_sleep_hold_dis();
    esp_sleep_config_gpio_isolate();

    //Wakeup on low and high can be used concurrently for separate pins
    esp_deep_sleep_enable_gpio_wakeup(1ULL << GPIO_SW_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
}


void app_main(void)
{
    init_nvs();
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    configure_gpios();
    start_bluetooth((bluetooth_config_t){
        .name = "Joystick",
        .uuid_16bit = (uint8_t[16]){0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00}
    });

    hid_demo_task(NULL);
}
