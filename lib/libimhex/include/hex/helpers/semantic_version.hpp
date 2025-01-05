#pragma once

#include <hex.hpp>

#include <compare>
#include <string>
#include <vector>

namespace hex {

    class SemanticVersion {
    public:
        SemanticVersion() = default;
        SemanticVersion(std::string version);
        SemanticVersion(std::string_view version);
        SemanticVersion(const char *version);

        std::strong_ordering operator<=>(const SemanticVersion &) const;
        bool operator==(const SemanticVersion &other) const;

        u32 major() const;
        u32 minor() const;
        u32 patch() const;
        bool nightly() const;
        const std::string& buildType() const;

        bool isValid() const;

        std::string get(bool withBuildType = true) const;

    private:
        std::vector<std::string> m_parts;
        std::string m_buildType;
    };

}