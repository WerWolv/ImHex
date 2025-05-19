#pragma once

#include <optional>
#include <string>

namespace hex::plugin::builtin {

std::optional<std::string> get_description(std::string buffer);

} // namespace hex::plugin::builtin
