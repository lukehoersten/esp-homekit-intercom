#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <hap.h>
#include <app_wifi.h>
#include <app_hap_setup_payload.h>

#include <led.h>
#include <lock.h>
#include <bell.h>
#include <intercom.h>

#define INTERCOM_TASK_PRIORITY 1
#define INTERCOM_TASK_STACKSIZE 4 * 1024
#define INTERCOM_TASK_NAME "hap_intercom"

static void intercom_accessory_init(void *p)
{
	hap_init(HAP_TRANSPORT_WIFI); /* Initialize the HAP core */

	hap_acc_cfg_t cfg = {
		.name = "Intercom",
		.manufacturer = "Luke Hoersten",
		.model = "Esp32Intercom01",
		.serial_num = "001122334455",
		.fw_rev = "0.1.0",
		.hw_rev = NULL,
		.pv = "1.1.0",
		.identify_routine = led_identify,
		.cid = HAP_CID_DOOR,
	};

	hap_acc_t *intercom_accessory = hap_acc_create(&cfg);

	uint8_t product_data[] = {'E', 'S', 'P', '3', '2', 'H', 'A', 'P'};
	hap_acc_add_product_data(intercom_accessory, product_data, sizeof(product_data));
	hap_acc_add_serv(intercom_accessory, bell_service_init());
	hap_acc_add_serv(intercom_accessory, lock_service_init());
	led_identify_service_init();

	hap_add_accessory(intercom_accessory); /* Add the Accessory to the HomeKit Database */

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

	hap_enable_mfi_auth(HAP_MFI_AUTH_HW); /* Enable Hardware MFi authentication (applicable only for MFi variant of SDK) */

	app_wifi_init(); /* Initialize Wi-Fi */
	hap_start();	 /* After all the initializations are done, start the HAP core */
	ESP_LOGI(TAG, "HAP started");

	app_wifi_start(portMAX_DELAY); /* Start Wi-Fi */
	ESP_LOGI(TAG, "WIFI started");

	vTaskDelete(NULL); /* The task ends here. The read/write callbacks will be invoked by the HAP Framework */
}

void app_main()
{
	xTaskCreate(intercom_accessory_init, INTERCOM_TASK_NAME, INTERCOM_TASK_STACKSIZE, NULL, INTERCOM_TASK_PRIORITY, NULL);
}
