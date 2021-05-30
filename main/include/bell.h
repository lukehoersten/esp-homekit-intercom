#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <hap.h>

bool is_bell_ringing(int val);

void intercom_bell_read(void *p);

void IRAM_ATTR intercom_bell_isr(void *arg);

void intercom_bell_timer_cb(TimerHandle_t timer);

void intercom_bell_isr_gpio_init();

void intercom_bell_adc_gpio_init();

hap_serv_t *intercom_bell_init();
