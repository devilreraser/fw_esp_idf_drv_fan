/* *****************************************************************************
 * File:   cmd_fan.c
 * Author: Dimitar Lilov
 *
 * Created on 2022 06 18
 * 
 * Description: ...
 * 
 **************************************************************************** */

/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "cmd_fan.h"
#include "drv_fan.h"

#include <string.h>

#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"

#include "argtable3/argtable3.h"

/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "cmd_fan"

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */

static struct {
    struct arg_str *duty;
    struct arg_str *index;
    struct arg_str *cmd;
    struct arg_end *end;
} fan_args;


/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */

/* *****************************************************************************
 * Functions
 **************************************************************************** */
static int command_fan(int argc, char **argv)
{
    ESP_LOGI(__func__, "argc=%d", argc);
    for (int i = 0; i < argc; i++)
    {
        ESP_LOGI(__func__, "argv[%d]=%s", i, argv[i]);
    }

    int nerrors = arg_parse(argc, argv, (void **)&fan_args);
    if (nerrors != ESP_OK)
    {
        arg_print_errors(stderr, fan_args.end, argv[0]);
        return ESP_FAIL;
    }

    const char* fan_number = fan_args.index->sval[0];
    const char* fan_duty = fan_args.duty->sval[0];

    drv_fan_e_index_t fan_index = atoi(fan_number);
    ESP_LOGI(TAG, "fan_index=%d", fan_index);
    
    int fan_pwm_duty = atoi(fan_duty);
    ESP_LOGI(TAG, "fan_index=%d", fan_index);
    
    //int gpio_index = drv_fan_get_gpio_num_from_configuration(fan_index);
    //ESP_LOGI(TAG, "gpio_index=%d", gpio_index);

    const char* fan_command = fan_args.cmd->sval[0];

    if (strcmp(fan_command,"start") == 0)
    {
        drv_fan_start(fan_index);
    }
    else if (strcmp(fan_command,"stop") == 0)
    {
        drv_fan_stop(fan_index);
    }
    else if (strcmp(fan_command,"duty") == 0)
    {
        drv_fan_pwm_duty(fan_index, fan_pwm_duty);
    }
    else
    {
        ESP_LOGE(TAG, "Unknown command %s", fan_command);
        return ESP_FAIL;
    }
    return 0;
}

static void register_fan(void)
{
    fan_args.duty  = arg_strn("d", "duty",   "<pwm duty>",           0, 1, "Specify fan duty cycle   : fan -d {0..100}");
    fan_args.index = arg_strn("i", "index",    "<index>",            1, 1, "Specify fan device index : fan -i {0|1}");
    fan_args.cmd   = arg_strn("c", "command",  "<command>",          1, 1, "Command can be    : fan -c {start|stop|duty}");
    fan_args.end   = arg_end(2);

    const esp_console_cmd_t cmd_fan = {
        .command = "fan",
        .help = "FAN Command Request",
        .hint = NULL,
        .func = &command_fan,
        .argtable = &fan_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_fan));
}


void cmd_fan_register(void)
{
    register_fan();
}
