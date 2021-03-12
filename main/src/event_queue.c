#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>

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
    ESP_LOGI(TAG, "Intercom event queued BELL RING");
    xQueueSendFromISR(intercom_event_queue, (void *)&INTERCOM_EVENT_QUEUE_BELL_RING, NULL);
}

void intercom_event_queue_lock_unsecure()
{
    ESP_LOGI(TAG, "Intercom event queued LOCK UNSECURE");
    xQueueSendToBack(intercom_event_queue, (void *)&INTERCOM_EVENT_QUEUE_LOCK_UNSECURE, 10);
}

void intercom_event_queue_lock_secure()
{
    ESP_LOGI(TAG, "Intercom event queued LOCK SECURE");
    xQueueSendToBack(intercom_event_queue, (void *)&INTERCOM_EVENT_QUEUE_LOCK_SECURE, 10);
}

void intercom_event_queue_lock_timeout()
{
    ESP_LOGI(TAG, "Intercom event queued LOCK TIMEOUT");
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
    int queue_len = 8;
    int queue_item_size = sizeof(uint8_t);
    intercom_event_queue = xQueueCreate(queue_len, queue_item_size);
    return intercom_event_queue != NULL;
}
