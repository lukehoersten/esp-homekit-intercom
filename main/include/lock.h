#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <hap.h>

void intercom_lock_unsecure();

void intercom_lock_secure();

void intercom_lock_timeout();

int intercom_lock_write_cb(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv);

void intercom_lock_timer_cb(TimerHandle_t timer);

hap_serv_t *intercom_lock_init(uint32_t key_gpio_pin);
