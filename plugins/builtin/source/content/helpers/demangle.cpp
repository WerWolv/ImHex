#include <content/helpers/demangle.hpp>
#include <llvm/Demangle/Demangle.h>

namespace hex::plugin::builtin {

    std::string demangle(const std::string &mangled) {
        std::string result = llvm::demangle(mangled);
        if (result.empty() || result == mangled)
            result = llvm::demangle("_" + mangled);

        if (result.empty() || result == ("_" + mangled))
            result = mangled;

        return result;
    }

}