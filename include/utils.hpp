#pragma once

#include <windows.h>
#include <shobjidl.h>

#include <locale>
#include <codecvt>
#include <optional>
#include <string>

namespace hex {

    std::optional<std::string> openFileDialog();

}