#pragma once

#include "esp_err.h"

typedef int gpio_num_t;
typedef int gpio_mode_t;
typedef int gpio_pull_mode_t;

#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ONLY 1

esp_err_t gpio_set_direction(gpio_num_t pin, gpio_mode_t mode);
esp_err_t gpio_set_pull_mode(gpio_num_t pin, gpio_pull_mode_t mode);
int gpio_get_level(gpio_num_t pin);
