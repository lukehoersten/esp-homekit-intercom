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

#define INTERCOM_BELL_TASK_PRIORITY 1
#define INTERCOM_BELL_TASK_STACKSIZE 4 * 1024
#define INTERCOM_BELL_TASK_NAME "hap_intercom_bell"

static TaskHandle_t intercom_bell_read_task;

volatile bool is_intercom_bell_blocked;
static TimerHandle_t intercom_bell_timer; // ignore new bells until timer triggered
static hap_char_t *intercom_bell_current_state;

void bell_hap_ring()
{
    ESP_LOGI(TAG, "Intercom bell HAP RING");
    hap_char_update_val(intercom_bell_current_state, &HAP_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS);
}

void bell_block()
{
    is_intercom_bell_blocked = true;
    ESP_LOGI(TAG, "Intercom bell updated [blocked: %s]", is_intercom_bell_blocked ? "true" : "false");
    xTimerReset(intercom_bell_timer, pdFALSE);
}

void bell_unblock()
{
    is_intercom_bell_blocked = false;
    ESP_LOGI(TAG, "Intercom bell updated [blocked: %s]", is_intercom_bell_blocked ? "true" : "false");
}

bool is_bell_blocked()
{
    ESP_LOGI(TAG, "Intercom bell triggered [blocked: %s]", is_intercom_bell_blocked ? "true" : "false");
    return is_intercom_bell_blocked;
}

int read_adc()
{
    int val = adc1_get_raw(ADC1_GPIO33_CHANNEL);
    ESP_LOGI(TAG, "Intercom bell read [val: %d]", val);
    return val;
}

bool is_bell_ringing()
{
    int val = read_adc();
    bool is_ringing = 2340 < val && val < 2360;
    ESP_LOGI(TAG, "Intercom bell rang [val: %d; is_ringing: %s]", val, is_ringing ? "true" : "false");
    return is_ringing;
}

int read_adc_avg()
{
    //new average = old average + (next data - old average) / next count
    int avg = 0;
    for (int i = 1; i < 10; i++)
    {
        avg = avg + ((read_adc() - avg) / i);
    }
    return avg;
}

void intercom_bell_read(void *p)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!is_bell_blocked() && is_bell_ringing())
        {
            bell_hap_ring();
            bell_block();
        }
    }
}

void IRAM_ATTR intercom_bell_isr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    configASSERT(intercom_bell_read_task != NULL);
    vTaskNotifyGiveFromISR(intercom_bell_read_task, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR();
}

void intercom_bell_timer_cb(TimerHandle_t timer)
{
    bell_unblock();
}

void intercom_bell_blocker_init()
{
    is_intercom_bell_blocked = false;
    intercom_bell_timer = xTimerCreate("intercom_bell_timer", pdMS_TO_TICKS(CONFIG_HOMEKIT_INTERCOM_LOCK_TIMEOUT),
                                       pdFALSE, 0, intercom_bell_timer_cb);
}

hap_serv_t *intercom_bell_service_init()
{
    hap_serv_t *intercom_bell_service = hap_serv_doorbell_create(0);
    hap_serv_add_char(intercom_bell_service, hap_char_name_create("Intercom Bell"));
    intercom_bell_current_state = hap_serv_get_char_by_uuid(intercom_bell_service, HAP_CHAR_UUID_PROGRAMMABLE_SWITCH_EVENT);
    return intercom_bell_service;
}

void intercom_bell_isr_gpio_init()
{
    // Configure ISR GPIO Pin 27
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;        /* Interrupt for falling edge  */
    io_conf.pin_bit_mask = GPIO_SEL_27;           /* Bit mask of the pins */
    io_conf.mode = GPIO_MODE_INPUT;               /* Set as input mode */
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     /* Disable internal pull-up */
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; /* Enable internal pull-down */
    gpio_config(&io_conf);                        /* Set the GPIO configuration */

    gpio_install_isr_service(0);                                     /* Install gpio isr service */
    gpio_isr_handler_add(GPIO_NUM_27, intercom_bell_isr, (void *)0); /* Hook isr handler for specified gpio pin */
}

void intercom_bell_adc_gpio_init()
{
    // Configure ADC1 Channel 5, GPIO Pin 33
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;        /* Interrupt for falling edge  */
    io_conf.pin_bit_mask = GPIO_SEL_33;           /* Bit mask of the pins */
    io_conf.mode = GPIO_MODE_INPUT;               /* Set as input mode */
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     /* Disable internal pull-up */
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; /* Enable internal pull-down */
    gpio_config(&io_conf);                        /* Set the GPIO configuration */

    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv425adc1_config_channel_atten14adc1_channel_t11adc_atten_t
    adc1_config_width(ADC_WIDTH_BIT_12); /* The value read is 12 bits wide (range 0-4095). */
    adc1_config_channel_atten(ADC1_GPIO33_CHANNEL, ADC_ATTEN_DB_11);
}

hap_serv_t *intercom_bell_init()
{
    xTaskCreate(intercom_bell_read, INTERCOM_BELL_TASK_NAME, INTERCOM_BELL_TASK_STACKSIZE, NULL,
                INTERCOM_BELL_TASK_PRIORITY, &intercom_bell_read_task);

    intercom_bell_blocker_init();
    intercom_bell_isr_gpio_init();
    intercom_bell_adc_gpio_init();
    return intercom_bell_service_init();
}
