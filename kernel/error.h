#pragma once

#include <stdint.h>

namespace kernel {

enum class GenericErrorCode {
    Success,
    ResponseTimeout,
    BadParameters,
    BadResponse,
    NotInitialized,
};

struct Error {
    GenericErrorCode generic_error_code;
    uint64_t device_specific_error_code;
    char const* user_message;
    void* extra_data;

    bool is_success() const { return generic_error_code == GenericErrorCode::Success; }
};

#define TRY(expr)                   \
    do {                            \
        auto __result = (expr);     \
        if (!__result.is_success()) \
            return __result;        \
    } while (0)

static constexpr Error Success { GenericErrorCode::Success, 0, "Success", nullptr };
static constexpr Error ResponseTimeout { GenericErrorCode::ResponseTimeout, 0, "Device did not response in time", nullptr };
static constexpr Error BadParameters { GenericErrorCode::BadParameters, 0, "Bad parameters", nullptr };
static constexpr Error DeviceNotInitialized { GenericErrorCode::NotInitialized, 0, "Device was not initialized before usage", nullptr };

}
