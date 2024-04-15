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
#include "cmd_fan.h"

#include "sdkconfig.h"
#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"

#include "drv_pwm_led.h"

/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "drv_fan"

#define MIN_PWM_DUTY_PERCENT_SPEED_READ_ABLE     16      /* minimum speed with tacho pin goes good to ground */
#define MIN_PWM_DUTY_PERCENT_ABSOLUTE_MIN        12      /* minimum speed (stop request) */

#define PWM_DUTY_PERCENT_INITIAL                MIN_PWM_DUTY_PERCENT_ABSOLUTE_MIN      /* on initialization */
#define PWM_DUTY_PERCENT_STOP_COMMAND           MIN_PWM_DUTY_PERCENT_SPEED_READ_ABLE   /* minimum speed (stop request) */

#define MAX_FAN_ENTRIES             2

#define MIN_EDGE_CHANGE_FILTER_US   (  1 * 1000)
#define MAX_TIME_BETWEEN_EDGES_US   (100 * 1000)

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
    int tacho_gpio_interrupts_per_mechanical_round;
    int speed_rpm;
    int speed_rpm_filtered;
    uint32_t count_interrupts;
    uint32_t last_time_us_interrupt;
    int64_t last_time_us_interrupt_prev;
    uint32_t last_time_us_read;
    uint32_t last_time_us_read_prev;

    drv_pwm_led_e_channel_t pwm_channel;

}drv_fan_s_entry_t;


/* *****************************************************************************
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */
drv_fan_s_entry_t fan_entry[MAX_FAN_ENTRIES] = {0};
bool b_isr_service_installed = false;
drv_pwm_led_e_timer_t pwm_fan_timer = DRV_PWM_LED_TIMER_MAX;

/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */
static void fan_speed_gpio_isr_handler(void* arg);

/* *****************************************************************************
 * Functions
 **************************************************************************** */
void drv_fan_cmd_register(void)
{
    cmd_fan_register();
}

void drv_fan_init(int fan_index, int pwm_gpio, int tacho_gpio, int tacho_change_per_round)
{
    if (fan_index < MAX_FAN_ENTRIES)
    {
        fan_entry[fan_index].pwm_gpio = pwm_gpio;
        fan_entry[fan_index].tacho_gpio = tacho_gpio;
        fan_entry[fan_index].tacho_gpio_interrupts_per_mechanical_round = tacho_change_per_round;
        fan_entry[fan_index].speed_rpm = 0;
        fan_entry[fan_index].count_interrupts = 0;
        fan_entry[fan_index].last_time_us_read_prev = esp_timer_get_time();
        fan_entry[fan_index].last_time_us_interrupt_prev = esp_timer_get_time();


        if (pwm_gpio != GPIO_NUM_NC)
        {
            #if CONFIG_DRV_PWM_LED_USE
            if (pwm_fan_timer == DRV_PWM_LED_TIMER_MAX)
            {
                pwm_fan_timer = drv_pwm_led_free_timer_get();
                drv_pwm_led_init_timer(pwm_fan_timer, 25000);                                                 /* 25000 Hz */
            }
            if (pwm_fan_timer < DRV_PWM_LED_TIMER_MAX)
            {
                drv_pwm_led_e_channel_t pwm_fan_channel = drv_pwm_led_free_channel_get();
                if (pwm_fan_channel < DRV_PWM_LED_CHANNEL_MAX)
                {
                    fan_entry[fan_index].pwm_channel = pwm_fan_channel;
                    drv_pwm_led_init(fan_entry[fan_index].pwm_channel, fan_entry[fan_index].pwm_gpio, pwm_fan_timer, PWM_DUTY_PERCENT_INITIAL, 0.0);             /* 0% duty 0% offset_set_high */
                }
                else
                {
                    ESP_LOGE(TAG, "pwm_fan_channel = DRV_PWM_LED_CHANNEL_MAX - cannot set pwm_gpio[%d]", fan_index);
                }
            }
            else
            {
                ESP_LOGE(TAG, "pwm_fan_timer = DRV_PWM_LED_TIMER_MAX - cannot set pwm_gpio[%d]", fan_index);
            }
            
            #endif

            //gpio_set_direction(pwm_gpio, GPIO_MODE_OUTPUT);
            
            //gpio_set_direction(pwm_gpio, GPIO_MODE_OUTPUT_OD);
            //gpio_set_pull_mode(pwm_gpio, GPIO_PULLUP_ONLY);
        }
        
        if (tacho_gpio != GPIO_NUM_NC)
        {
            gpio_set_direction(tacho_gpio, GPIO_MODE_INPUT);
            gpio_set_pull_mode(tacho_gpio, GPIO_PULLUP_ONLY);
            if (b_isr_service_installed == false)
            {
                gpio_install_isr_service(ESP_INTR_FLAG_IRAM); // Choose an appropriate interrupt flag
                b_isr_service_installed = true;
            }
            gpio_isr_handler_add(tacho_gpio, fan_speed_gpio_isr_handler, (void*) tacho_gpio);
            gpio_set_intr_type(tacho_gpio, GPIO_INTR_ANYEDGE);
        }



    }
}
void drv_fan_pwm_duty(int fan_index, int percent)
{
    if (fan_index < MAX_FAN_ENTRIES)
    {
        //gpio_set_level(fan_entry[fan_index].pwm_gpio, 0);
        drv_pwm_led_set_duty(fan_entry[fan_index].pwm_channel, percent);
        ESP_LOGI(TAG, "Set %d duty for FAN[%d]", percent, fan_index);
    }
}

void drv_fan_start(int fan_index)
{
    if (fan_index < MAX_FAN_ENTRIES)
    {
        //gpio_set_level(fan_entry[fan_index].pwm_gpio, 0);
        drv_pwm_led_set_duty(fan_entry[fan_index].pwm_channel, 100.0);
    }
}

void drv_fan_stop(int fan_index)
{
    if (fan_index < MAX_FAN_ENTRIES)
    {
        //gpio_set_level(fan_entry[fan_index].pwm_gpio, 1);
        drv_pwm_led_set_duty(fan_entry[fan_index].pwm_channel, PWM_DUTY_PERCENT_STOP_COMMAND);
    }
}

void drv_fan_tacho_disable(void)
{
    for (int index = 0; index < MAX_FAN_ENTRIES; index++)
    {
        if (fan_entry[index].tacho_gpio_interrupts_per_mechanical_round)
        {
            if (fan_entry[index].tacho_gpio != GPIO_NUM_NC)
            {
                if (gpio_intr_disable(fan_entry[index].tacho_gpio) != ESP_OK)
                {
                    ESP_LOGE(TAG, "drv_fan_disable of tacho pin %d failure", fan_entry[index].tacho_gpio);
                }
                else
                {
                    ESP_LOGI(TAG, "drv_fan_disable of tacho pin %d success", fan_entry[index].tacho_gpio);
                }
            }
        }
    }
}

void drv_fan_tacho_enable(void)
{
    for (int index = 0; index < MAX_FAN_ENTRIES; index++)
    {
        if (fan_entry[index].tacho_gpio_interrupts_per_mechanical_round)
        {
            if (fan_entry[index].tacho_gpio != GPIO_NUM_NC)
            {
                if (gpio_intr_enable(fan_entry[index].tacho_gpio) != ESP_OK)
                {
                    ESP_LOGE(TAG, "drv_fan_enable of tacho pin %d failure", fan_entry[index].tacho_gpio);
                }
                else
                {
                    ESP_LOGI(TAG, "drv_fan_enable of tacho pin %d success", fan_entry[index].tacho_gpio);
                }
            }
        }
    }
}

int drv_fan_get_speed_rpm(int fan_index)
{
    int64_t curr_read_time = esp_timer_get_time();
    int interrupts_count = fan_entry[fan_index].count_interrupts;
    fan_entry[fan_index].count_interrupts = 0;

    fan_entry[fan_index].last_time_us_read = curr_read_time - fan_entry[fan_index].last_time_us_read_prev;
    fan_entry[fan_index].last_time_us_read_prev = curr_read_time;
    fan_entry[fan_index].speed_rpm = ((int64_t)interrupts_count * 1000000 * 60) / (fan_entry[fan_index].tacho_gpio_interrupts_per_mechanical_round * fan_entry[fan_index].last_time_us_read);

    ESP_LOGI(TAG, "FAN[%d] last_time_us_read:%5u interrupts:%5d rpm:%d", fan_index, (unsigned int)fan_entry[fan_index].last_time_us_read, interrupts_count, fan_entry[fan_index].speed_rpm);
    return fan_entry[fan_index].speed_rpm;
}



void fan_speed_gpio_isr_handler(void* arg)
{
    int tacho_pin = (int) arg;

    for (int index = 0; index < MAX_FAN_ENTRIES; index++)
    {
        if (tacho_pin == fan_entry[index].tacho_gpio)
        {
            int64_t curr_interrupt_time = esp_timer_get_time();
            fan_entry[index].last_time_us_interrupt = curr_interrupt_time - fan_entry[index].last_time_us_interrupt_prev;
            fan_entry[index].last_time_us_interrupt_prev = curr_interrupt_time;
            if (fan_entry[index].last_time_us_interrupt > MIN_EDGE_CHANGE_FILTER_US)
            {
                fan_entry[index].count_interrupts++;
            }
        }
    }
}

