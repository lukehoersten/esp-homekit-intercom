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

#define INTERCOM_LOCK_GPIO_LOCKED 0
#define INTERCOM_LOCK_GPIO_UNLOCKED 1

#define HAP_LOCK_CURRENT_STATE_UNSECURED 0
#define HAP_LOCK_CURRENT_STATE_SECURED 1

#define HAP_LOCK_TARGET_STATE_UNSECURED 0
#define HAP_LOCK_TARGET_STATE_SECURED 1

static hap_val_t HAP_VAL_LOCK_CURRENT_STATE_UNSECURED = {.u = HAP_LOCK_CURRENT_STATE_UNSECURED};
static hap_val_t HAP_VAL_LOCK_CURRENT_STATE_SECURED = {.u = HAP_LOCK_CURRENT_STATE_SECURED};
static hap_val_t HAP_VAL_LOCK_TARGET_STATE_SECURED = {.u = HAP_LOCK_TARGET_STATE_SECURED};

static TimerHandle_t intercom_lock_timer; // lock the door when timer triggered
static hap_char_t *intercom_lock_current_state;
static hap_char_t *intercom_lock_target_state;

void intercom_lock_unsecure()
{
    ESP_LOGI(TAG, "Intercom lock unsecure");
    gpio_set_level(CONFIG_HOMEKIT_INTERCOM_LOCK_GPIO_PIN, INTERCOM_LOCK_GPIO_UNLOCKED);
    hap_char_update_val(intercom_lock_current_state, &HAP_VAL_LOCK_CURRENT_STATE_UNSECURED);
    xTimerReset(intercom_lock_timer, 10);
}

void intercom_lock_secure()
{
    ESP_LOGI(TAG, "Intercom lock secure");
    gpio_set_level(CONFIG_HOMEKIT_INTERCOM_LOCK_GPIO_PIN, INTERCOM_LOCK_GPIO_LOCKED);
    hap_char_update_val(intercom_lock_current_state, &HAP_VAL_LOCK_CURRENT_STATE_SECURED);
}

int intercom_lock_write_cb(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++)
    {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_LOCK_TARGET_STATE))
        {
            ESP_LOGI(TAG, "Intercom lock received write [%d]", write->val.u);

            switch (write->val.u)
            {
            case HAP_LOCK_TARGET_STATE_UNSECURED:
                intercom_lock_unsecure();
                break;
            case HAP_LOCK_TARGET_STATE_SECURED:
                intercom_lock_secure();
                break;
            }

            /* Update target state */
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

void intercom_lock_timer_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "Intercom lock timer fired");
    intercom_lock_secure();
    hap_char_update_val(intercom_lock_target_state, &HAP_VAL_LOCK_TARGET_STATE_SECURED);
}

hap_serv_t *intercom_lock_init(uint32_t key_gpio_pin)
{
    hap_serv_t *intercom_lock_service = hap_serv_lock_mechanism_create(HAP_VAL_LOCK_CURRENT_STATE_SECURED.u, HAP_LOCK_TARGET_STATE_SECURED);
    hap_serv_add_char(intercom_lock_service, hap_char_name_create("Intercom Lock"));

    intercom_lock_current_state = hap_serv_get_char_by_uuid(intercom_lock_service, HAP_CHAR_UUID_LOCK_CURRENT_STATE);
    intercom_lock_target_state = hap_serv_get_char_by_uuid(intercom_lock_service, HAP_CHAR_UUID_LOCK_TARGET_STATE);

    hap_serv_set_write_cb(intercom_lock_service, intercom_lock_write_cb); /* Set the write callback for the service */

    intercom_lock_timer = xTimerCreate("intercom_lock_timer", pdMS_TO_TICKS(CONFIG_HOMEKIT_INTERCOM_LOCK_TIMEOUT), pdFALSE, 0, intercom_lock_timer_cb);

    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;       /* Disable interrupt  */
    io_conf.pin_bit_mask = 1ULL << key_gpio_pin; /* Bit mask of the pins */
    io_conf.mode = GPIO_MODE_OUTPUT;             /* Set as input mode */
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;    /* Disable internal pull-up */
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE; /* Enable internal pull-down */

    gpio_config(&io_conf); /* Set the GPIO configuration */

    return intercom_lock_service;
}
