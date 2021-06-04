#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include <lock.h>
#include <intercom.h>

#define LOCK_UNSECURED 0
#define LOCK_SECURED 1

static TimerHandle_t lock_auto_secure_timer; // lock the door when timer triggered
static hap_char_t *lock_current_state;
static hap_char_t *lock_target_state;

void lock_update_current_state(uint8_t is_secured)
{
    ESP_LOGI(TAG, "lock updated [%s]", is_secured ? "secured" : "unsecured");
    gpio_set_level(GPIO_NUM_21, !is_secured); // the GPIO pin is inverse secure/unsecure of homekit.

    hap_val_t val = {.u = is_secured};
    hap_char_update_val(lock_current_state, &val);

    if (!is_secured)
        xTimerReset(lock_auto_secure_timer, 10);
}

int lock_write_cb(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++)
    {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_LOCK_TARGET_STATE))
        {
            lock_update_current_state(write->val.u);
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
        }
        else
        {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    return ret;
}

void lock_auto_secure_timer_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "lock timer fired");
    lock_update_current_state(LOCK_SECURED);
    hap_val_t val = {.u = LOCK_SECURED};
    hap_char_update_val(lock_target_state, &val);
}

void lock_gpio_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = GPIO_SEL_21;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io_conf);
}

hap_serv_t *lock_service_init()
{
    hap_serv_t *lock_service = hap_serv_lock_mechanism_create(LOCK_SECURED, LOCK_SECURED);
    hap_serv_add_char(lock_service, hap_char_name_create("Intercom Lock"));

    lock_current_state = hap_serv_get_char_by_uuid(lock_service, HAP_CHAR_UUID_LOCK_CURRENT_STATE);
    lock_target_state = hap_serv_get_char_by_uuid(lock_service, HAP_CHAR_UUID_LOCK_TARGET_STATE);

    hap_serv_set_write_cb(lock_service, lock_write_cb);

    lock_auto_secure_timer = xTimerCreate("lock_auto_secure_timer", pdMS_TO_TICKS(CONFIG_HOMEKIT_INTERCOM_LOCK_TIMEOUT), pdFALSE, 0, lock_auto_secure_timer_cb);

    lock_gpio_init();

    return lock_service;
}
