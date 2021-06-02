#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <soc/adc_channel.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include <intercom.h>
#include <bell.h>

static hap_val_t HAP_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS = {.u = 0};

#define BELL_TASK_PRIORITY 1
#define BELL_TASK_STACKSIZE 4 * 1024
#define BELL_TASK_NAME "hap_intercom_bell"

static TaskHandle_t bell_read_task_handle;
static TimerHandle_t bell_block_timer_handle; // ignore new bells until timer triggered
volatile bool is_bell_blocked;
static hap_char_t *bell_current_state;

void bell_rang()
{
    ESP_LOGI(TAG, "bell HAP RING");
    hap_char_update_val(bell_current_state, &HAP_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS);

    is_bell_blocked = true;
    ESP_LOGI(TAG, "bell timer set; bell updated [blocked: true]");
    xTimerReset(bell_block_timer_handle, pdFALSE);
}

bool is_bell_ringing()
{
    int val = adc1_get_raw(ADC1_GPIO33_CHANNEL);
    bool is_ringing = 1935 < val && val < 1945;
    ESP_LOGI(TAG, "bell rang [val: %d; is_ringing: %s]", val, is_ringing ? "true" : "false");
    return is_ringing;
}

void bell_read_task(void *p)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (is_bell_ringing())
            bell_rang();
    }
}

void IRAM_ATTR bell_isr(void *arg)
{
    if (!is_bell_blocked)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        configASSERT(bell_read_task_handle != NULL);
        vTaskNotifyGiveFromISR(bell_read_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR();
    }
}

void bell_block_timer_cb(TimerHandle_t timer)
{
    is_bell_blocked = false;
    ESP_LOGI(TAG, "bell timer triggered; bell updated [blocked: false]");
}

void bell_blocker_init()
{
    is_bell_blocked = false;
    bell_block_timer_handle = xTimerCreate("intercom_bell_timer", pdMS_TO_TICKS(CONFIG_HOMEKIT_INTERCOM_LOCK_TIMEOUT),
                                           pdFALSE, 0, bell_block_timer_cb);
}

hap_serv_t *bell_init()
{
    hap_serv_t *bell_service = hap_serv_doorbell_create(0);
    hap_serv_add_char(bell_service, hap_char_name_create("Intercom Bell"));
    bell_current_state = hap_serv_get_char_by_uuid(bell_service, HAP_CHAR_UUID_PROGRAMMABLE_SWITCH_EVENT);
    return bell_service;
}

void bell_isr_gpio_init()
{
    // Configure ISR GPIO Pin 27
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_SEL_27;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_27, bell_isr, (void *)0);
}

void bell_adc_gpio_init()
{
    // Configure ADC1 Channel 5, GPIO Pin 33
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = GPIO_SEL_33;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv425adc1_config_channel_atten14adc1_channel_t11adc_atten_t
    adc1_config_width(ADC_WIDTH_BIT_12); /* The value read is 12 bits wide (range 0-4095). */
    adc1_config_channel_atten(ADC1_GPIO33_CHANNEL, ADC_ATTEN_DB_11);
}

hap_serv_t *bell_service_init()
{
    xTaskCreate(bell_read_task, BELL_TASK_NAME, BELL_TASK_STACKSIZE, NULL,
                BELL_TASK_PRIORITY, &bell_read_task_handle);

    bell_blocker_init();
    bell_isr_gpio_init();
    bell_adc_gpio_init();
    return bell_init();
}
