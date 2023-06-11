#pragma once

#include <api/syscalls.h>
#include <kernel/error.h>

namespace kernel {

Error datetime_init();

Error datetime_read(api::DateTime& datetime);

Error datetime_set(api::DateTime& datetime);

}
