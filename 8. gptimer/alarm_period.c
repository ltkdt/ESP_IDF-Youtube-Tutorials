/*

Press the button the first time and the buzzer will play two seconds later, periodically. Press it again to make it stop. 

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
#define BUZZER_GPIO GPIO_NUM_32

uint64_t time_cnt = 0;
uint8_t press_cnt = 0;
gptimer_handle_t gptimer = NULL;
TaskHandle_t BuzzerAlarmHandle = NULL;
QueueHandle_t StateQueue;
bool AlarmState = false;

int alarm_target = 2; // 2 seconds then alarm

static void IRAM_ATTR isr_handler(void* arg)
{ 
    press_cnt++;
    if(press_cnt % 2){
        // Alarm is started from the point we start the button so we start the timer here
        ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer,0));
        ESP_ERROR_CHECK(gptimer_start(gptimer));
    }
    else{
        AlarmState = 0;
        xQueueSendFromISR(StateQueue, &AlarmState, NULL);
        ESP_ERROR_CHECK(gptimer_stop(gptimer));
    }
}

static bool alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_awoken = pdFALSE;
    AlarmState = !AlarmState;
    xQueueSendFromISR(StateQueue, &AlarmState, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

void BuzzerAlarm(void *arg)
{
    while(1){
        if(xQueueReceive(StateQueue, &AlarmState, 2000)){
            gpio_set_level(BUZZER_GPIO, AlarmState);
        }
    }
}

void app_main(void)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, 
    };
    
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0, // counter will reload with 0 on alarm event
        .alarm_count = alarm_target * 1E6, // 1s
        .flags.auto_reload_on_alarm = true, // enable auto-reload
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    gptimer_event_callbacks_t event_callback = {
        .on_alarm = alarm_callback, // register user callback
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &event_callback, StateQueue));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));

    esp_rom_gpio_pad_select_gpio(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
    
    esp_rom_gpio_pad_select_gpio(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
 
    gpio_pullup_en(BUTTON_GPIO);
 
    gpio_pulldown_dis(BUTTON_GPIO);
    gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, isr_handler , NULL);
    gpio_intr_enable(BUTTON_GPIO);

    // Create the queue
    StateQueue = xQueueCreate(3, sizeof(char));

    xTaskCreate(BuzzerAlarm, "BuzzerAlarm", 8192, NULL, 10, &BuzzerAlarmHandle);  
}
