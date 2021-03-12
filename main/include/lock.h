#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/adc.h>

#include <hap.h>

#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include <app_wifi.h>
#include <app_hap_setup_payload.h>

void intercom_lock_unsecure();

void intercom_lock_secure();

void intercom_lock_timeout();

int intercom_lock_write_cb(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv);

void intercom_lock_timer_cb(TimerHandle_t timer);

hap_serv_t *intercom_lock_init(uint32_t key_gpio_pin);
