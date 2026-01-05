#pragma once

#include <string>

namespace hex::crash {

    using CrashCallback = void (*) (const std::string&);

    void setCrashCallback(CrashCallback callback);
    void setupCrashHandlers();

}