idf_component_register(SRCS "cmd_fan.c" "drv_fan.c" 
                    INCLUDE_DIRS "." 
                    REQUIRES
                                driver
                                console
                                drv_pwm_led
                                      )
                 

