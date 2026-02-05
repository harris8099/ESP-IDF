/*
 * MIT License
 * 
 * Copyright (c) 2018 Michele Biondi
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "dht11.h"
#include "soc/gpio_num.h"
#include "tasks_common.h"
#include <stdio.h>

static gpio_num_t dht_gpio;
static int64_t last_read_time = -2000000;
static struct dht11_reading last_read;

static const char* TAG = "DHT";
struct dht11_reading DHT11_reading_t = {DHT11_TIMEOUT_ERROR, -1, -1};

static int _waitOrTimeout(uint16_t microSeconds, int level) {
    int micros_ticks = 0;
    while(gpio_get_level(dht_gpio) == level) { 
        if(micros_ticks++ > microSeconds) 
            return DHT11_TIMEOUT_ERROR;
        ets_delay_us(1);
    }
    return micros_ticks;
}

static int _checkCRC(uint8_t data[]) {
    if(data[4] == (data[0] + data[1] + data[2] + data[3]))
        return DHT11_OK;
    else
        return DHT11_CRC_ERROR;
}

static void _sendStartSignal() {
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 0);
    ets_delay_us(20 * 1000);
    gpio_set_level(dht_gpio, 1);
    ets_delay_us(40);
    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT);
}

static int _checkResponse() {
    /* Wait for next step ~80us*/
    if(_waitOrTimeout(80, 0) == DHT11_TIMEOUT_ERROR)
        return DHT11_TIMEOUT_ERROR;

    /* Wait for next step ~80us*/
    if(_waitOrTimeout(80, 1) == DHT11_TIMEOUT_ERROR) 
        return DHT11_TIMEOUT_ERROR;

    return DHT11_OK;
}

static struct dht11_reading _timeoutError() {
    struct dht11_reading timeoutError = {DHT11_TIMEOUT_ERROR, -1, -1};
    return timeoutError;
}

static struct dht11_reading _crcError() {
    struct dht11_reading crcError = {DHT11_CRC_ERROR, -1, -1};
    return crcError;
}

void DHT11_init(gpio_num_t gpio_num) {
    /* Wait 1 seconds to make the device pass its initial unstable status */
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    dht_gpio = gpio_num;
}

struct dht11_reading DHT11_read() {
    /* Tried to sense too son since last read (dht11 needs ~2 seconds to make a new read) */
    if(esp_timer_get_time() - 2000000 < last_read_time) {
        return last_read;
    }

    last_read_time = esp_timer_get_time();

    uint8_t data[5] = {0,0,0,0,0};

    _sendStartSignal();

    if(_checkResponse() == DHT11_TIMEOUT_ERROR)
        return last_read = _timeoutError();
    
    /* Read response */
    for(int i = 0; i < 40; i++) {
        /* Initial data */
        if(_waitOrTimeout(50, 0) == DHT11_TIMEOUT_ERROR)
            return last_read = _timeoutError();
                
        if(_waitOrTimeout(70, 1) > 28) {
            /* Bit received was a 1 */
            data[i/8] |= (1 << (7-(i%8)));
        }
    }

    if(_checkCRC(data) != DHT11_CRC_ERROR) {
        last_read.status = DHT11_OK;
        last_read.temperature = data[2];
        last_read.humidity = data[0];
        return last_read;
    } else {
        return last_read = _crcError();
    }
}


int getTemperature(void)
{
	return DHT11_reading_t.temperature;
}
int getHumidity(void)
{
	return DHT11_reading_t.humidity;
}


/*
	DHT11 Sensor task
*/
static void DHT11_task(void *pvParameter)
{
	gpio_num_t gpio	=	DHT_GPIO;
	DHT11_init(gpio);
	
	printf("Starting DHT task\n\n");
	
	for(;;)
	{
//		printf("=== Reading DHT ===\n");
		DHT11_reading_t = DHT11_read();
		
		switch (DHT11_reading_t.status) {
			case DHT11_CRC_ERROR:
				ESP_LOGI(TAG, "DHT11_CRC_ERROR");
				break;
			
			case DHT11_TIMEOUT_ERROR:
				ESP_LOGI(TAG, "DHT11_TIMEOUT_ERROR");
				break;
			
			case DHT11_OK:
				ESP_LOGI(TAG, "DHT11_OK");
				break;
		
			default:
				ESP_LOGI(TAG, "Unknown error");
		}

		printf("Hum %d\n", DHT11_reading_t.humidity);
		printf("Tmp %d\n", DHT11_reading_t.temperature);
		
		// Wait at least 2 seconds before reading again
		// The interval of the whole process must be more than 2 seconds
		vTaskDelay(4000/ portTICK_PERIOD_MS);
		
		
	}
}

void DHT11_task_start(void){
	xTaskCreatePinnedToCore(&DHT11_task, "DHT11_task", DHT11_TASK_STACK_SIZE, NULL, DHT11_TASK_PRIORITY, NULL, DHT11_TASK_CORE_ID);
}
