#pragma once
/*
A standalone hexpat file parser to extract the ‘#pragma description’.
Used to speed up “Import Pattern File…” in the “Pattern Editor” by avoiding
having to preprocess every pattern using pattern_language to extract the it.
*/

#include <optional>
#include <string>

namespace hex::plugin::builtin {

std::optional<std::string> get_description(std::string buffer);

} // namespace hex::plugin::builtin
