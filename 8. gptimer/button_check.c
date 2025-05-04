/*

Check if a button is pressed then print the time since the timer is started till you press the button.
If you press more than once then the result is the time interval since you last pressed the button till now.
Please note that normal 4-pin pushbutton is sensitive and may record multiple input when you press the button, this code perform eliniminating noise
by not registering a press count if the time interval is smaller than 10 ms

*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#define BUTTON_GPIO GPIO_NUM_33

uint64_t last = 0;
uint64_t now = 0;
uint8_t press_cnt = 0;
uint32_t debounce_delay = 10000;

static const char *TAG = "Timer example";

gptimer_handle_t gptimer = NULL;
TaskHandle_t ButtonCheckHandle = NULL;

static void IRAM_ATTR isr_handler(void* arg)
{ 
    ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &now));       // Get current time
    ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer, 0));          // Reset counter to 0 
    if(now - last > debounce_delay || last - now > debounce_delay){                      // Check for bounce input
        last = now;
        press_cnt++;
    }
}

void ButtonCheck(void *arg)
{
    while (press_cnt < 10)               
    {
        double seconds = (double)last * 1E-6;
        ESP_LOGI(TAG, "Last time button pressed (in microseconds): %.3f", seconds);             
        // Using logging library of ESP_IDF, this is just like printf but it needs TAG as the first arguement
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Stops the timer and free memory when we no longer use it

    ESP_LOGI(TAG, "Stop timer");
    ESP_ERROR_CHECK(gptimer_stop(gptimer));
    ESP_LOGI(TAG, "Disable timer");
    ESP_ERROR_CHECK(gptimer_disable(gptimer));
    ESP_LOGI(TAG, "Delete timer");
    ESP_ERROR_CHECK(gptimer_del_timer(gptimer));

    // Delete the task
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Timer config
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick = 1us
    };

    // Create the timer
    
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    // Note that enable doenst start the timer right away , it only changes the state from init to enable. You need to start it
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    // The below code is similar to the second video on GPIO interrupt.

    esp_rom_gpio_pad_select_gpio(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
 
    gpio_pullup_en(BUTTON_GPIO);
 
    gpio_pulldown_dis(BUTTON_GPIO);

     /* Raising edge signal will trigger our interrupt */
    gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_POSEDGE);

    /* install gpio isr service to default values */
    gpio_install_isr_service(0);

    /* Attach the ISR to the GPIO interrupt */
    gpio_isr_handler_add(BUTTON_GPIO, isr_handler , NULL);

    /* Enable the Interrupt */
    gpio_intr_enable(BUTTON_GPIO);

    xTaskCreate(ButtonCheck, "ButtonCheck", 8192, NULL, 10, &ButtonCheckHandle); 

    // We don't want to trigger watchdog timer reset in this example. A watchdog timer reset occur when program crashes or in idle for too long 
    ESP_ERROR_CHECK(esp_task_wdt_deinit());
    
}



