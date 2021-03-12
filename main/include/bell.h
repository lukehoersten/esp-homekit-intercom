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

bool is_bell_ringing(int val);

void IRAM_ATTR intercom_bell_isr(void *arg);

void intercom_bell_ring();

void intercom_bell_timer_cb(TimerHandle_t timer);

hap_serv_t *intercom_bell_init(uint32_t key_gpio_pin);
