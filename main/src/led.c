#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <hap.h>

#include <led.h>
#include <intercom.h>

#define LED_ON 1
#define LED_OFF 0
#define LED_DELAY 500
#define LED_NUM_BLINK 3

int intercom_led_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Accessory identified");
    for (int i = 0; i < LED_NUM_BLINK; i++)
    {
        gpio_set_level(GPIO_NUM_13, LED_ON);
        vTaskDelay(pdMS_TO_TICKS(LED_DELAY));
        gpio_set_level(GPIO_NUM_13, LED_OFF);
        vTaskDelay(pdMS_TO_TICKS(LED_DELAY));
    }
    return HAP_SUCCESS;
}

void intercom_led_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;       /* Interrupt for falling edge  */
    io_conf.pin_bit_mask = GPIO_SEL_13;          /* Bit mask of the pins */
    io_conf.mode = GPIO_MODE_OUTPUT;             /* Set as input mode */
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;    /* Disable internal pull-up */
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE; /* Enable internal pull-down */
    gpio_config(&io_conf);                       /* Set the GPIO configuration */
}