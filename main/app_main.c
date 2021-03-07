/* HomeKit Door Homekit
 */
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

static const char *TAG = "HAP door";

#define DOOR_TASK_PRIORITY  1
#define DOOR_TASK_STACKSIZE 4 * 1024
#define DOOR_TASK_NAME      "hap_door"

#define RESET_NETWORK_BUTTON_TIMEOUT 3

#define DOOR_LOCK_GPIO_LOCKED 0
#define DOOR_LOCK_GPIO_UNLOCKED 1

#define HAP_LOCK_TARGET_STATE_UNSECURED 0
#define HAP_LOCK_TARGET_STATE_SECURED 1
static hap_val_t HAP_LOCK_CURRENT_STATE_UNSECURED = {.u = 0};
static hap_val_t HAP_LOCK_CURRENT_STATE_SECURED = {.u = 1};
static hap_val_t HAP_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS = {.u = 0};

#define ESP_INTR_FLAG_DEFAULT 0

static const uint8_t DOOR_EVENT_QUEUE_BELL = 1;
static const uint8_t DOOR_EVENT_QUEUE_UNLOCK = 2;
static const uint8_t DOOR_EVENT_QUEUE_LOCK = 3;
static const uint8_t DOOR_EVENT_QUEUE_LOCK_TIMEOUT = 4;

static uint8_t tlv8buff[128];
static hap_data_val_t null_tlv8 = { .buf = &tlv8buff, .buflen = 127 };

static xQueueHandle door_event_queue = NULL;
static TimerHandle_t door_lock_timer = NULL;

/**
 * @brief the recover door bell gpio interrupt function
 */
static void IRAM_ATTR door_bell_isr(void* arg) {
	xQueueSendFromISR(door_event_queue, (void*) &DOOR_EVENT_QUEUE_BELL, NULL);
}

/**
 * Enable a GPIO Pin for Door Bell
 */
static void door_bell_init(uint32_t key_gpio_pin) {
	gpio_config_t io_conf;

	io_conf.intr_type		= GPIO_INTR_NEGEDGE;	 /* Interrupt for falling edge  */
	io_conf.pin_bit_mask	= 1 << key_gpio_pin;	 /* Bit mask of the pins */
	io_conf.mode			= GPIO_MODE_INPUT;	 /* Set as input mode */
	io_conf.pull_up_en	= GPIO_PULLUP_DISABLE;	 /* Disable internal pull-up */
	io_conf.pull_down_en	= GPIO_PULLDOWN_ENABLE;		 /* Enable internal pull-down */

	gpio_config(&io_conf);	 /* Set the GPIO configuration */

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);	 /* Install gpio isr service */
	gpio_isr_handler_add(key_gpio_pin, door_bell_isr, (void*)key_gpio_pin);		 /* Hook isr handler for specified gpio pin */
}

/**
 * Enable a GPIO Pin for Door Lock
 */
static void door_lock_init(uint32_t key_gpio_pin) {
	gpio_config_t io_conf;

	io_conf.intr_type		= GPIO_INTR_DISABLE;	 /* Interrupt for falling edge  */
	io_conf.pin_bit_mask	= 1 << key_gpio_pin;	 /* Bit mask of the pins */
	io_conf.mode			= GPIO_MODE_OUTPUT;	 /* Set as input mode */
	io_conf.pull_up_en	= GPIO_PULLUP_DISABLE;	 /* Disable internal pull-up */
	io_conf.pull_down_en	= GPIO_PULLDOWN_ENABLE;		 /* Enable internal pull-down */

	gpio_config(&io_conf);	 /* Set the GPIO configuration */
}

/**
 * Enable a GPIO Pin for LED
 */
static void led_init(uint32_t key_gpio_pin) {
	gpio_config_t io_conf;

	io_conf.intr_type		= GPIO_INTR_DISABLE;	 /* Interrupt for falling edge  */
	io_conf.pin_bit_mask	= 1 << key_gpio_pin;	 /* Bit mask of the pins */
	io_conf.mode			= GPIO_MODE_OUTPUT;	 /* Set as input mode */
	io_conf.pull_up_en	= GPIO_PULLUP_DISABLE;	 /* Disable internal pull-up */
	io_conf.pull_down_en	= GPIO_PULLDOWN_ENABLE;		 /* Enable internal pull-down */

	gpio_config(&io_conf);	 /* Set the GPIO configuration */
}

static void reset_network_handler(void* arg) {
	ESP_LOGI(TAG, "Resetting network");
	hap_reset_network();
}

static void reset_init(uint32_t key_gpio_pin) {
	button_handle_t handle = iot_button_create(key_gpio_pin, BUTTON_ACTIVE_LOW);
	iot_button_add_on_release_cb(handle, RESET_NETWORK_BUTTON_TIMEOUT, reset_network_handler, NULL);
}

/**
 * Initialize the Door Hardware. Here, we just enebale the Door Bell detection.
 */
void door_hardware_init(gpio_num_t reset_gpio_num, gpio_num_t door_bell_gpio_num, gpio_num_t door_lock_gpio_num, gpio_num_t led_gpio_num) {
	int queue_len = 4;
	int queue_item_size = sizeof(uint8_t);

	door_event_queue = xQueueCreate(queue_len, queue_item_size);
	if (door_event_queue != NULL) {
		/* reset_init(reset_gpio_num); */
		door_bell_init(door_bell_gpio_num);
		door_lock_init(door_lock_gpio_num);
		led_init(led_gpio_num);

	}
}

/* Mandatory identify routine for the accessory.
 * In a real accessory, something like LED blink should be implemented
 * got visual identification
 */
static int door_identify(hap_acc_t *ha) {
	ESP_LOGI(TAG, "Accessory identified");

	for (int i = 0; i < 3; i++) {
		gpio_set_level(CONFIG_HOMEKIT_DOOR_LED_GPIO_PIN, 1);
		vTaskDelay(pdMS_TO_TICKS(500));
		gpio_set_level(CONFIG_HOMEKIT_DOOR_LED_GPIO_PIN, 0);
		vTaskDelay(pdMS_TO_TICKS(500));
	}

	return HAP_SUCCESS;
}

static void door_bell_ring(hap_char_t *door_bell_current_state) {
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_0);
	int val = adc1_get_raw(ADC1_CHANNEL_2);

	/* int level = gpio_get_level(CONFIG_HOMEKIT_DOOR_BELL_GPIO_PIN); */

	ESP_LOGI(TAG, "Door bell ring event processed [%d]", val);

	/* hap_char_update_val(door_bell_current_state, &HAP_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS); */
}

static void door_unlock(hap_char_t *door_lock_current_state) {
	ESP_LOGI(TAG, "Door unlock event processed");

	gpio_set_level(CONFIG_HOMEKIT_DOOR_LOCK_GPIO_PIN, DOOR_LOCK_GPIO_UNLOCKED);
	hap_char_update_val(door_lock_current_state, &HAP_LOCK_CURRENT_STATE_UNSECURED);

	xTimerReset(door_lock_timer, 10);
}

static void door_lock(hap_char_t *door_lock_current_state) {
	ESP_LOGI(TAG, "Door lock event processed");

	gpio_set_level(CONFIG_HOMEKIT_DOOR_LOCK_GPIO_PIN, DOOR_LOCK_GPIO_LOCKED);
	hap_char_update_val(door_lock_current_state, &HAP_LOCK_CURRENT_STATE_SECURED);
}

static void door_lock_timeout(hap_char_t *door_lock_target_state) {
	ESP_LOGI(TAG, "Door lock timeout event processed");

	xQueueSendToBack(door_event_queue, (void*) &DOOR_EVENT_QUEUE_LOCK, 10);
	hap_val_t target_lock_secured = {.u = HAP_LOCK_TARGET_STATE_SECURED};
	hap_char_update_val(door_lock_target_state, &target_lock_secured);
}

static int door_lock_write_cb(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv) {
	int i, ret = HAP_SUCCESS;
	hap_write_data_t *write;
	for (i = 0; i < count; i++) {
		write = &write_data[i];
		if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_LOCK_TARGET_STATE)) {
			ESP_LOGI(TAG, "Received Write. Door lock %d", write->val.u);

			switch (write->val.u) {
				case HAP_LOCK_TARGET_STATE_UNSECURED:
					xQueueSendToBack(door_event_queue, (void*) &DOOR_EVENT_QUEUE_UNLOCK, 10);
					break;
				case HAP_LOCK_TARGET_STATE_SECURED:
					xQueueSendToBack(door_event_queue, (void*) &DOOR_EVENT_QUEUE_LOCK, 10);
					break;
			}

			/* Update target state */
			hap_char_update_val(write->hc, &(write->val));
			*(write->status) = HAP_STATUS_SUCCESS;
		} else {
			*(write->status) = HAP_STATUS_RES_ABSENT;
		}
	}
	return ret;
}

static void door_lock_timer_cb(TimerHandle_t timer) {
	ESP_LOGI(TAG, "Door lock timer fired - event queued");

	xQueueSendToBack(door_event_queue, (void*) &DOOR_EVENT_QUEUE_LOCK_TIMEOUT, 10);
}

/*The main thread for handling the Door Accessory */
static void door_thread_entry(void *p) {
	hap_init(HAP_TRANSPORT_WIFI);	 /* Initialize the HAP core */

	/* Initialise the mandatory parameters for Accessory which will be added as
	 * the mandatory services internally
	 */
	hap_acc_cfg_t cfg = {
		.name = "Door",
		.manufacturer = "Luke Hoersten",
		.model = "Esp32Door01",
		.serial_num = "001122334455",
		.fw_rev = "0.1.0",
		.hw_rev = NULL,
		.pv = "1.1.0",
		.identify_routine = door_identify,
		.cid = HAP_CID_DOOR,
	};

	hap_acc_t *door_accessory = hap_acc_create(&cfg);	 /* Create accessory object */

	/* Add a dummy Product Data */
	uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
	hap_acc_add_product_data(door_accessory, product_data, sizeof(product_data));

	hap_serv_t *door_bell_service = hap_serv_doorbell_create(0);
	hap_serv_add_char(door_bell_service, hap_char_name_create("Doorbell"));
	hap_char_t *door_bell_current_state = hap_serv_get_char_by_uuid(door_bell_service, HAP_CHAR_UUID_PROGRAMMABLE_SWITCH_EVENT);

	hap_serv_t *door_lock_service = hap_serv_lock_mechanism_create(HAP_LOCK_CURRENT_STATE_SECURED.u, HAP_LOCK_TARGET_STATE_SECURED);
	hap_serv_add_char(door_lock_service, hap_char_name_create("Door Lock"));
	hap_char_t *door_lock_current_state = hap_serv_get_char_by_uuid(door_lock_service, HAP_CHAR_UUID_LOCK_CURRENT_STATE);
	hap_char_t *door_lock_target_state = hap_serv_get_char_by_uuid(door_lock_service, HAP_CHAR_UUID_LOCK_TARGET_STATE);

	/* Get pointer to the door in use characteristic which we need to monitor for state changes */
	hap_serv_set_write_cb(door_lock_service, door_lock_write_cb);	 /* Set the write callback for the service */

	hap_acc_add_serv(door_accessory, door_bell_service);
	hap_acc_add_serv(door_accessory, door_lock_service);

	hap_add_accessory(door_accessory);	 /* Add the Accessory to the HomeKit Database */

	/* Initialize the appliance specific hardware. This enables out-in-use detection */
	door_hardware_init(CONFIG_HOMEKIT_DOOR_WIFI_RESET_GPIO_PIN, CONFIG_HOMEKIT_DOOR_BELL_GPIO_PIN, CONFIG_HOMEKIT_DOOR_LOCK_GPIO_PIN, CONFIG_HOMEKIT_DOOR_LED_GPIO_PIN);

	/* For production accessories, the setup code shouldn't be programmed on to
	 * the device. Instead, the setup info, derived from the setup code must
	 * be used. Use the factory_nvs_gen utility to generate this data and then
	 * flash it into the factory NVS partition.
	 *
	 * By default, the setup ID and setup info will be read from the factory_nvs
	 * Flash partition and so, is not required to set here explicitly.
	 *
	 * However, for testing purpose, this can be overridden by using hap_set_setup_code()
	 * and hap_set_setup_id() APIs, as has been done here.
	 */
#ifdef CONFIG_HOMEKIT_USE_HARDCODED_SETUP_CODE
	/* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
	hap_set_setup_code(CONFIG_HOMEKIT_SETUP_CODE);
	/* Unique four character Setup Id. Default: ES32 */
	hap_set_setup_id(CONFIG_HOMEKIT_SETUP_ID);
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
	app_hap_setup_payload(CONFIG_HOMEKIT_SETUP_CODE, CONFIG_HOMEKIT_SETUP_ID, true, cfg.cid);
#else
	app_hap_setup_payload(CONFIG_HOMEKIT_SETUP_CODE, CONFIG_HOMEKIT_SETUP_ID, false, cfg.cid);
#endif
#endif

	hap_enable_mfi_auth(HAP_MFI_AUTH_HW);	 /* Enable Hardware MFi authentication (applicable only for MFi variant of SDK) */

	app_wifi_init();	 /* Initialize Wi-Fi */
	hap_start();	 /* After all the initializations are done, start the HAP core */
	app_wifi_start(portMAX_DELAY);	 /* Start Wi-Fi */

	door_lock_timer = xTimerCreate("door_lock_timer", pdMS_TO_TICKS(CONFIG_HOMEKIT_DOOR_LOCK_TIMEOUT), pdFALSE, 0, door_lock_timer_cb);

	/* Listen for doorbell state change events. Other read/write functionality will be handled by the HAP Core.  When the
	 * doorbell in Use GPIO goes low, it means doorbell is not ringing.  When the Door in Use GPIO goes high, it means
	 * the doorbell is ringing.  Applications can define own logic as per their hardware.
	 */
	uint8_t door_event_queue_item = DOOR_EVENT_QUEUE_LOCK;

	while (1) {
		if (xQueueReceive(door_event_queue, &door_event_queue_item, portMAX_DELAY) == pdFALSE) {
			ESP_LOGI(TAG, "Door event queue trigger FAIL");
		} else {
			switch(door_event_queue_item) {
				case DOOR_EVENT_QUEUE_BELL:
					door_bell_ring(door_bell_current_state);
					break;
				case DOOR_EVENT_QUEUE_UNLOCK:
					door_unlock(door_lock_current_state);
					break;
				case DOOR_EVENT_QUEUE_LOCK:
					door_lock(door_lock_current_state);
					break;
				case DOOR_EVENT_QUEUE_LOCK_TIMEOUT:
					door_lock_timeout(door_lock_target_state);
					break;
			}
		}
	}
}

void app_main() {
	xTaskCreate(door_thread_entry, DOOR_TASK_NAME, DOOR_TASK_STACKSIZE, NULL, DOOR_TASK_PRIORITY, NULL);
}
