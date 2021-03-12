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

#include <event_queue.h>
#include <bell.h>

#define ESP_INTR_FLAG_DEFAULT 0

static hap_val_t HAP_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS = {.u = 0};

volatile bool is_intercom_bell_blocked;
static TimerHandle_t intercom_bell_timer; // ignore new bells until timer triggered
static hap_char_t *intercom_bell_current_state;

bool is_bell_ringing(int val)
{
    return 1.2 < val && val < 2.5;
}

void IRAM_ATTR intercom_bell_isr(void *arg)
{
    if (!is_intercom_bell_blocked && is_bell_ringing(adc1_get_raw(CONFIG_HOMEKIT_INTERCOM_BELL_ADC1_CHANNEL)))
    {
        intercom_event_queue_bell_ring();
        is_intercom_bell_blocked = true;
    }
}

void intercom_bell_ring()
{
    ESP_LOGI(TAG, "Intercom bell ring event processed");

    hap_char_update_val(intercom_bell_current_state, &HAP_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS);

    xTimerReset(intercom_bell_timer, 10);
}

void intercom_bell_timer_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "Intercom bell timer fired");
    is_intercom_bell_blocked = false;
}

hap_serv_t *intercom_bell_init(uint32_t key_gpio_pin)
{
    is_intercom_bell_blocked = false;
    intercom_bell_timer = xTimerCreate("intercom_bell_timer", pdMS_TO_TICKS(CONFIG_HOMEKIT_INTERCOM_LOCK_TIMEOUT), pdFALSE, 0, intercom_bell_timer_cb);

    hap_serv_t *intercom_bell_service = hap_serv_doorbell_create(0);
    hap_serv_add_char(intercom_bell_service, hap_char_name_create("Intercom Bell"));
    intercom_bell_current_state = hap_serv_get_char_by_uuid(intercom_bell_service, HAP_CHAR_UUID_PROGRAMMABLE_SWITCH_EVENT);

    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_POSEDGE;       /* Interrupt for rising edge  */
    io_conf.pin_bit_mask = 1 << key_gpio_pin;    /* Bit mask of the pins */
    io_conf.mode = GPIO_MODE_INPUT;              /* Set as input mode */
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;    /* Disable internal pull-up */
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE; /* Enable internal pull-down */

    gpio_config(&io_conf); /* Set the GPIO configuration */

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);                             /* Install gpio isr service */
    gpio_isr_handler_add(key_gpio_pin, intercom_bell_isr, (void *)key_gpio_pin); /* Hook isr handler for specified gpio pin */

    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv425adc1_config_channel_atten14adc1_channel_t11adc_atten_t
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(CONFIG_HOMEKIT_INTERCOM_BELL_ADC1_CHANNEL, ADC_ATTEN_DB_11);

    return intercom_bell_service;
}