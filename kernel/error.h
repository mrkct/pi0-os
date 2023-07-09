#pragma once

#include <kernel/panic.h>
#include <stdint.h>

namespace kernel {

enum class GenericErrorCode {
    Success,
    ResponseTimeout,
    BadParameters,
    BadResponse,
    NotInitialized,
    NotConnected,
    DeviceNotReady,
    DeviceIsBusy,
    OutOfMemory,
    CantRepeat,
    NotSupported,
    InvalidFormat,
    EndOfData,
    NotFound,
    NotAFile,
    NotADirectory,
    PathTooLong,
    InvalidSystemCall,
    NotImplemented,
};

static constexpr char const* __generic_error_code_to_string(GenericErrorCode code)
{
    switch (code) {
    case GenericErrorCode::Success:
        return "Success";
    case GenericErrorCode::ResponseTimeout:
        return "ResponseTimeout";
    case GenericErrorCode::BadParameters:
        return "BadParameters";
    case GenericErrorCode::BadResponse:
        return "BadResponse";
    case GenericErrorCode::NotInitialized:
        return "NotInitialized";
    case GenericErrorCode::NotConnected:
        return "NotConnected";
    case GenericErrorCode::DeviceNotReady:
        return "DeviceNotReady";
    case GenericErrorCode::DeviceIsBusy:
        return "DeviceIsBusy";
    case GenericErrorCode::OutOfMemory:
        return "OutOfMemory";
    case GenericErrorCode::CantRepeat:
        return "CantRepeat";
    case GenericErrorCode::NotSupported:
        return "NotSupported";
    case GenericErrorCode::InvalidFormat:
        return "InvalidFormat";
    case GenericErrorCode::EndOfData:
        return "EndOfData";
    case GenericErrorCode::NotFound:
        return "NotFound";
    case GenericErrorCode::NotAFile:
        return "NotAFile";
    case GenericErrorCode::NotADirectory:
        return "NotADirectory";
    case GenericErrorCode::InvalidSystemCall:
        return "InvalidSystemCall";
    case GenericErrorCode::PathTooLong:
        return "PathTooLong";
    case GenericErrorCode::NotImplemented:
        return "NotImplemented";
    default:
        return "Unknown";
    }
}

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

#define MUST(expr)                                                                        \
    do {                                                                                  \
        auto __result = (expr);                                                           \
        if (!__result.is_success())                                                       \
            panic(                                                                        \
                "The following expression returned an error:\n" #expr "\n"                \
                "Called from %s:%d\n"                                                     \
                "| Error: %s\n"                                                           \
                "| DeviceSpecificError: %lu (%lx)\n"                                      \
                "| Message: \"%s\"\n"                                                     \
                "| Extra data available: %s\n",                                           \
                __FILE__, __LINE__,                                                       \
                __generic_error_code_to_string(__result.generic_error_code),              \
                __result.device_specific_error_code, __result.device_specific_error_code, \
                __result.user_message,                                                    \
                __result.extra_data == nullptr ? "no" : "yes");                           \
    } while (0)

#define TODO() panic("TODO: %s:%d\n", __FILE__, __LINE__)

static constexpr Error Success { GenericErrorCode::Success, 0, "Success", nullptr };
static constexpr Error ResponseTimeout { GenericErrorCode::ResponseTimeout, 0, "Device did not response in time", nullptr };
static constexpr Error BadParameters { GenericErrorCode::BadParameters, 0, "Bad parameters", nullptr };
static constexpr Error DeviceNotInitialized { GenericErrorCode::NotInitialized, 0, "Device was not initialized before usage", nullptr };
static constexpr Error DeviceNotConnected { GenericErrorCode::NotConnected, 0, "Device is not connected", nullptr };
static constexpr Error DeviceNotReady { GenericErrorCode::DeviceNotReady, 0, "Device is not yet ready, retry the operation", nullptr };
static constexpr Error DeviceIsBusy { GenericErrorCode::DeviceIsBusy, 0, "Device is busy, retry the operation", nullptr };
static constexpr Error OutOfMemory { GenericErrorCode::OutOfMemory, 0, "Out of memory", nullptr };
static constexpr Error CantRepeat { GenericErrorCode::CantRepeat, 0, "Can't repeat the operation", nullptr };
static constexpr Error NotSupported { GenericErrorCode::NotSupported, 0, "Not supported", nullptr };
static constexpr Error InvalidFormat { GenericErrorCode::InvalidFormat, 0, "Invalid format", nullptr };
static constexpr Error EndOfData { GenericErrorCode::EndOfData, 0, "End of data", nullptr };
static constexpr Error NotFound { GenericErrorCode::NotFound, 0, "Not found", nullptr };
static constexpr Error NotAFile { GenericErrorCode::NotAFile, 0, "Not a file", nullptr };
static constexpr Error NotADirectory { GenericErrorCode::NotADirectory, 0, "Not a directory", nullptr };
static constexpr Error InvalidSystemCall { GenericErrorCode::InvalidSystemCall, 0, "Invalid system call", nullptr };
static constexpr Error PathTooLong { GenericErrorCode::PathTooLong, 0, "Path too long", nullptr };
static constexpr Error NotImplemented { GenericErrorCode::NotImplemented, 0, "Not implemented", nullptr };

}
