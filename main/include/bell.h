#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <hap.h>

bool is_bell_ringing(int val);

void IRAM_ATTR intercom_bell_isr(void *arg);

void intercom_bell_ring();

void intercom_bell_timer_cb(TimerHandle_t timer);

hap_serv_t *intercom_bell_init(uint32_t key_gpio_pin);
