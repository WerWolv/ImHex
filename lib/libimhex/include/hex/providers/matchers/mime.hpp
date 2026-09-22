#pragma once

#include <hex/helpers/magic.hpp>

namespace hex::prv {

    class PatternMatcherMIME : public PatternMatcher<"MIME"> {
    public:
        PatternMatcherMIME(Provider *provider) : PatternMatcher(provider) {
            m_mimeType = magic::getMIMEType(provider, 0, 4 * 1024, true);
        }
        bool match(const std::string& parameter) override {
            return magic::isValidMIMEType(parameter) && parameter == m_mimeType;
        }

    private:
        std::string m_mimeType;
    };

}