#pragma once

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

#define SCRIPT_API_IMPL(VERSION, ReturnAndName, ...) extern "C" [[maybe_unused, gnu::visibility("default")]] CONCAT(ReturnAndName, VERSION) (__VA_ARGS__)
#define SCRIPT_API(ReturnAndName, ...) SCRIPT_API_IMPL(VERSION, ReturnAndName, __VA_ARGS__)