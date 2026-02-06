/*
	Application entry point
*/
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "dht11.h"
#include "wifi_app.h"
#include "wifi_reset_button.h"

void app_main(void)
{
    // Initialize NVS 
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
	// Start wifi
	wifi_app_start();
	
	// Configure WIfi reset button
	wifi_reset_button_config();
	
	// Start DHT11 sensor task
	DHT11_task_start();
}
