#pragma once

#include <kernel/device/aux.h>
#include <kernel/error.h>
#include "keyboard.h"

namespace kernel {

Error virtinput_keyboard_init(void);

}