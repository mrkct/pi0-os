#pragma once

#include <api/syscalls.h>
#include <kernel/error.h>

namespace kernel {

Error datetime_init();

Error datetime_read(DateTime& datetime);

Error datetime_set(DateTime& datetime);

}
