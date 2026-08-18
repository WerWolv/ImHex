#pragma once

#include <hex/providers/provider.hpp>

namespace hex::prv {

    class PatternMatcherFileName : public PatternMatcher<"filename"> {
    public:
        PatternMatcherFileName(Provider *provider) : PatternMatcher(provider) {
            if (const auto fileBacked = dynamic_cast<IProviderFileBacked *>(provider)) {
                m_paths = fileBacked->getBackedFiles();
            }
        }

        bool match(const std::string& parameter) override {
            if (isTooBroadFileMatch(parameter))
                return false;

            std::string prefix, suffix, wholeMatch;

            const auto parts = wolv::util::splitString(parameter, "*", false);
            if (parts.size() == 1) {
                wholeMatch = parts[0];
            } else if (parts.size() == 2) {
                prefix = parts[0];
                suffix = parts[1];
            } else {
                return false;
            }

            if (m_paths.empty())
                return false;

            for (const auto &path : m_paths) {
                const auto fileName = wolv::util::toUTF8String(path.filename());
                if (!wholeMatch.empty() && fileName != wholeMatch)
                    continue;
                if (!prefix.empty() && !fileName.starts_with(prefix))
                    continue;
                if (!suffix.empty() && !fileName.ends_with(suffix))
                    continue;

                return true;
            }

            return false;
        }

    private:
        static bool isTooBroadFileMatch(const std::string &match) {
            return
                match == "*.bin" ||
                match == "*.dat";
        }

    private:
        std::set<std::fs::path> m_paths;
    };

}