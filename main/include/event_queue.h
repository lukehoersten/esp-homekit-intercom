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

static const char *TAG = "HAP Intercom";

void intercom_event_queue_bell_ring();

void intercom_event_queue_lock_unsecure();

void intercom_event_queue_lock_secure();

void intercom_event_queue_lock_timeout();

void intercom_event_queue_run();

bool intercom_event_queue_init();
