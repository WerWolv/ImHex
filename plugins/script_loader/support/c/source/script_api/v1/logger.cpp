#include <script_api.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/logger.hpp>

#define VERSION V1

SCRIPT_API(void logPrint, const char *message) {
    hex::log::print("{}", message);
}

SCRIPT_API(void logPrintln, const char *message) {
    hex::log::println("{}", message);
}

SCRIPT_API(void logDebug, const char *message) {
    hex::log::debug("{}", message);
}

SCRIPT_API(void logInfo, const char *message) {
    hex::log::info("{}", message);
}

SCRIPT_API(void logWarn, const char *message) {
    hex::log::warn("{}", message);
}

SCRIPT_API(void logError, const char *message) {
    hex::log::error("{}", message);
}

SCRIPT_API(void logFatal, const char *message) {
    hex::log::fatal("{}", message);
}