#pragma once

namespace hex::prv {

    class PatternMatcherProviderType : public PatternMatcher<"data-source"> {
    public:
        PatternMatcherProviderType(Provider *provider) : PatternMatcher(provider) { }

        bool match(const std::string& parameter) override {
            return getProvider()->getTypeName() == UnlocalizedString(parameter);
        }
    };

}