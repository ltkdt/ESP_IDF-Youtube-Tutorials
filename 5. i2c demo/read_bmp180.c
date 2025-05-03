/*

Read calibration value from BMP180 and find the temperature. 
 
Datasheet and coding guide

Datasheet: https://cdn-shop.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf
Summary: Basically you read the calibration data from BMP180 like below, and uncompensated data, then follow the coding guide to perform calculation

Coding guide: 
https://controllerstech.com/interface-bmp180-with-stm32/ (this is for STM32 but you it is relatively easy to convert it to the code for ESP-IDF)
https://esp32tutorials.com/bmp180-esp32-esp-idf/ (using legacy i2c driver of esp_idf, you need to convert it to the current version)


CMakeLists.txt:
REQUIRES    esp_driver_i2c

Wiring: 
BMP180 - 3.3V supply
SDA - GPIO 21
SCL - GPIO 22
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "math.h"

#define I2C_MASTER_TIMEOUT_MS       1000

#define START_READ_CALC_A1          0xAA  // Calibration data (16 bits)

#define BMP180_CONTROL_REGISTER             0xF4  // Control register
#define BMP180_RESULTS       0xF6  // Read results here
#define BMP180_REQUEST_TEMP_CMD       0x2E  // Request temperature measurement
#define BMP180_REQUEST_PRESSURE_CMD   0x34  // Request pressure measurement

TaskHandle_t BMP180ReadHandle = NULL;

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

static void i2c_master_init_handle(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t address){
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

// Read bytes from I2C device but you need to specify the register address to read from and len or how many bytes you will read. The results is parsed into the "data" array 
static esp_err_t read_byte_i2c(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Write a byte to the I2C register address of the device 
static esp_err_t write_byte_i2c(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data}; // The packet must have an address followed by the data
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void read_bmp_180_task(void *arg){
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init_bus(&bus_handle);

    i2c_master_init_handle(&bus_handle, &dev_handle, 0x77);

    uint8_t data_cali[22] = {0}; // 22 bytes of calibration data
    uint8_t data_temp[2] = {0};  // 2 bytes of uncompensated temperature data

    int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
    uint16_t ac4, ac5, ac6;

    while(1){
        // Start reading calibration data
        read_byte_i2c(dev_handle, START_READ_CALC_A1, data_cali, 22);
        ac1 = (int16_t) (data_cali[0] << 8) | data_cali[1];
        ac2 = (int16_t) (data_cali[2] << 8) | data_cali[3];
        ac3 = (int16_t) (data_cali[4] << 8) | data_cali[5];
        ac4 = (uint16_t) (data_cali[6] << 8) | data_cali[7];
        ac5 = (uint16_t) (data_cali[8] << 8) | data_cali[9];
        ac6 = (uint16_t) (data_cali[10] << 8) | data_cali[11];
        b1 = (int16_t) (data_cali[12] << 8) | data_cali[13];
        b2 = (int16_t) (data_cali[14] << 8) | data_cali[15];
        mb = (int16_t) (data_cali[16] << 8) | data_cali[17];
        mc = (int16_t) (data_cali[18] << 8) | data_cali[19];
        md = (int16_t) (data_cali[20] << 8) | data_cali[21];

        printf("Value read: %d \n", ac1);
        printf("Value read: %d \n", ac2);
        printf("Value read: %d \n", ac3);
        printf("Value read: %d \n", ac4);
        printf("Value read: %d \n", ac5);
        printf("Value read: %d \n", ac6);
        printf("Value read: %d \n", b1);
        printf("Value read: %d \n", b2);
        printf("Value read: %d \n", mb);
        printf("Value read: %d \n", mc);
        printf("Value read: %d \n", md);

        printf("\n");

        // Write a request command (BMP180_REQUEST_TEMP_CMD) to the control register (0x4F)
        write_byte_i2c(dev_handle, BMP180_CONTROL_REGISTER, BMP180_REQUEST_TEMP_CMD);

        // Wait for 5 ms then read from the register 0xF6 which gives us raw data
        vTaskDelay(5 / portTICK_PERIOD_MS);
        read_byte_i2c(dev_handle, BMP180_RESULTS, data_temp, 2);

        uint16_t raw_temp = (uint16_t) (data_temp[0] << 8) | data_temp[1];

        // Perform calculation according to the datasheet
        float x1 = ((raw_temp - ac6) * (ac5/(pow(2,15))));
	    float x2 = ((mc*(pow(2,11))) / (x1+md));
	    float b5 = x1 + x2;
	    float true_temp = (b5+8)/(pow(2,4));
        true_temp /= 10.0;

        printf("The temperature recorded after calibration: %f", true_temp);
        printf("\n");

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}

void app_main(void){
    xTaskCreatePinnedToCore(read_bmp_180_task, "Scan I2C", 4098, NULL, 10, &BMP180ReadHandle, 1);  // Core 1
}