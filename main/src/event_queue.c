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

#include <lock.h>
#include <bell.h>
#include <event_queue.h>

static const int INTERCOM_EVENT_QUEUE_BELL_RING = 1;
static const int INTERCOM_EVENT_QUEUE_LOCK_UNSECURE = 2;
static const int INTERCOM_EVENT_QUEUE_LOCK_SECURE = 3;
static const int INTERCOM_EVENT_QUEUE_LOCK_TIMEOUT = 4;

static xQueueHandle intercom_event_queue = NULL;

void intercom_event_queue_bell_ring()
{
    xQueueSendFromISR(intercom_event_queue, (void *)&INTERCOM_EVENT_QUEUE_BELL_RING, NULL);
}

void intercom_event_queue_lock_unsecure()
{
    xQueueSendToBack(intercom_event_queue, (void *)&INTERCOM_EVENT_QUEUE_LOCK_UNSECURE, 10);
}

void intercom_event_queue_lock_secure()
{
    xQueueSendToBack(intercom_event_queue, (void *)&INTERCOM_EVENT_QUEUE_LOCK_SECURE, 10);
}

void intercom_event_queue_lock_timeout()
{
    xQueueSendToBack(intercom_event_queue, (void *)&INTERCOM_EVENT_QUEUE_LOCK_TIMEOUT, 10);
}

void intercom_event_queue_run()
{
    uint8_t intercom_event_queue_item = INTERCOM_EVENT_QUEUE_LOCK_SECURE;

    while (1)
    {
        if (xQueueReceive(intercom_event_queue, &intercom_event_queue_item, portMAX_DELAY) == pdFALSE)
        {
            ESP_LOGI(TAG, "Intercom event queue trigger FAIL");
        }
        else
        {
            switch (intercom_event_queue_item)
            {
            case INTERCOM_EVENT_QUEUE_BELL_RING:
                intercom_bell_ring();
                break;
            case INTERCOM_EVENT_QUEUE_LOCK_UNSECURE:
                intercom_lock_unsecure();
                break;
            case INTERCOM_EVENT_QUEUE_LOCK_SECURE:
                intercom_lock_secure();
                break;
            case INTERCOM_EVENT_QUEUE_LOCK_TIMEOUT:
                intercom_lock_timeout();
                break;
            }
        }
    }
}

bool intercom_event_queue_init()
{
    int queue_len = 4;
    int queue_item_size = sizeof(uint8_t);
    intercom_event_queue = xQueueCreate(queue_len, queue_item_size);
    return intercom_event_queue != NULL;
}
