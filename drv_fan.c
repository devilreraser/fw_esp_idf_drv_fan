/* *****************************************************************************
 * File:   drv_fan.c
 * Author: XX
 *
 * Created on YYYY MM DD
 * 
 * Description: ...
 * 
 **************************************************************************** */

/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "drv_fan.h"

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"
/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "drv_fan"




#define MAX_FAN_ENTRIES 2

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */
typedef struct
{
    int pwm_gpio;           /* if -1 - not used */
    int tacho_gpio;         /* if -1 - not used */
    int tacho_gpio_level_changes_per_mechanical_round;
    int64_t last_time_us_since_boot;
    int speed_rpm;
    int count_edges;
    int64_t last_time_us_between_edges;
    int64_t last_time_us_speed_read;
}drv_fan_s_entry_t;


/* *****************************************************************************
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */
drv_fan_s_entry_t fan_entry[MAX_FAN_ENTRIES] = {0};
bool b_isr_service_installed = false;

/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */
static void fan_speed_gpio_isr_handler(void* arg);

/* *****************************************************************************
 * Functions
 **************************************************************************** */
void drv_fan_init(int fan_index, int pwm_gpio, int tacho_gpio, int tacho_change_per_round)
{
    if (fan_index < MAX_FAN_ENTRIES)
    {
        fan_entry[fan_index].pwm_gpio = pwm_gpio;
        fan_entry[fan_index].tacho_gpio = tacho_gpio;
        fan_entry[fan_index].tacho_gpio_level_changes_per_mechanical_round = tacho_change_per_round;
        fan_entry[fan_index].last_time_us_since_boot = esp_timer_get_time();
        fan_entry[fan_index].speed_rpm = 0;
        fan_entry[fan_index].count_edges = 0;


        if (pwm_gpio != GPIO_NUM_NC)
        {
            gpio_set_direction(pwm_gpio, GPIO_MODE_OUTPUT_OD);
        }
        
        if (tacho_gpio != GPIO_NUM_NC)
        {
            gpio_set_direction(tacho_gpio, GPIO_MODE_INPUT);
            if (b_isr_service_installed == false)
            {
                gpio_install_isr_service(ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM); // Choose an appropriate interrupt flag
                b_isr_service_installed = true;
            }
            gpio_set_intr_type(tacho_gpio, GPIO_INTR_ANYEDGE);
            gpio_isr_handler_add(tacho_gpio, fan_speed_gpio_isr_handler, (void*) tacho_gpio);
        }



    }
}


int drv_fan_get_speed_rpm(int fan_index)
{
    ESP_LOGI(TAG, "FAN[%d] time_us:%5d edges:%5d last_us:%d", fan_index, (int)fan_entry[fan_index].last_time_us_between_edges, fan_entry[fan_index].count_edges, (int)fan_entry[fan_index].last_time_us_since_boot);
    int64_t curr_read_time = esp_timer_get_time();
    if (fan_entry[fan_index].count_edges)
    {
        fan_entry[fan_index].speed_rpm =  ((int64_t)fan_entry[fan_index].count_edges * 1000000 * 60 / 4) / (curr_read_time - fan_entry[fan_index].last_time_us_speed_read);
    }
    else
    {
        fan_entry[fan_index].speed_rpm = 0;
    }
    fan_entry[fan_index].last_time_us_speed_read = curr_read_time;
    fan_entry[fan_index].count_edges = 0;
    return fan_entry[fan_index].speed_rpm;
}

static void fan_speed_gpio_isr_handler(void* arg)
{
    int64_t curr_interrupt_read_time = esp_timer_get_time();
    int tacho_pin = (int) arg;

    for (int index = 0; index < MAX_FAN_ENTRIES; index++)
    {
        if (tacho_pin == fan_entry[index].tacho_gpio)
        {
            fan_entry[index].count_edges++;
            fan_entry[index].last_time_us_between_edges = (curr_interrupt_read_time - fan_entry[index].last_time_us_since_boot);
            fan_entry[index].speed_rpm = fan_entry[index].last_time_us_between_edges / fan_entry[index].tacho_gpio_level_changes_per_mechanical_round / 10000000 / 60;
            fan_entry[index].last_time_us_since_boot = curr_interrupt_read_time;
        }
    }
}

