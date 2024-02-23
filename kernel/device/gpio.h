#pragma once

#include <kernel/error.h>

namespace kernel {

enum class PinFunction {
    Input = 0,
    Output = 1,
    Alternate0 = 0b100,
    Alternate1 = 0b101,
    Alternate2 = 0b110,
    Alternate3 = 0b111,
    Alternate4 = 0b011,
    Alternate5 = 0b010,
};

Error gpio_set_pin_function(uint8_t pin, PinFunction);
Error gpio_set_pin_high_detect_enable(uint8_t pin, bool);
int gpio_read_pin(uint8_t pin);

enum class PullUpDownState {
    Disable = 0,
    PullDown = 1,
    PullUp = 2,
};

Error gpio_set_pin_pull_up_down_state(uint8_t pin, PullUpDownState);

}
