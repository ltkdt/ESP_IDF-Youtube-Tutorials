/*
This code uses ESP-IDF 5.4 to scan the the address of I2C device

CMakeLists.txt:
REQUIRES    esp_driver_i2c

Wiring: 
SDA - GPIO 21
SCL - GPIO 22
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define I2C_MASTER_TIMEOUT_MS       1000

TaskHandle_t CheckAddressHandle = NULL;

// Function for initializing I2C bus
static void i2c_master_init_bus(i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
}

// Function for initializing I2C handle 
static void i2c_master_init_handle(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t address){
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

void check_address_task(void *arg){
    i2c_master_bus_handle_t *bus_handle = (i2c_master_bus_handle_t*) arg;
    while(1){
        esp_err_t err;
        // Using i2c_master_probe on a for loop going through all available address
        for(uint8_t i = 3; i < 0x78; i++){
            err = i2c_master_probe(*bus_handle, i, 1000);
            if (err == ESP_OK){
                printf("I2C Scanner found I2C device at: 0x%X \n", i);
            }
        }
        printf("I2C_Scanner complete \n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void){
    i2c_master_bus_handle_t bus_handle;
    i2c_master_init_bus(&bus_handle);
    xTaskCreatePinnedToCore(check_address_task, "Scan I2C", 16384, (void *) bus_handle, 10, &CheckAddressHandle, 1);  // Core 1
}