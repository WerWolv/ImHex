#include <hex/helpers/fmt.hpp>
#include <hex/helpers/semantic_version.hpp>
#include <wolv/utils/string.hpp>

namespace hex {

    SemanticVersion::SemanticVersion(const char *version) : SemanticVersion(std::string(version)) {

    }

    SemanticVersion::SemanticVersion(std::string_view version) : SemanticVersion(std::string(version.begin(), version.end())) {

    }

    SemanticVersion::SemanticVersion(std::string version) {
        if (version.empty())
            return;

        if (version.starts_with("v"))
            version = version.substr(1);

        m_parts = wolv::util::splitString(version, ".");

        if (m_parts.size() != 3 && m_parts.size() != 4) {
            m_parts.clear();
            return;
        }

        if (m_parts.back().contains("-")) {
            auto buildTypeParts = wolv::util::splitString(m_parts.back(), "-");
            if (buildTypeParts.size() != 2) {
                m_parts.clear();
                return;
            }

            m_parts.back() = buildTypeParts[0];
            m_buildType = buildTypeParts[1];
        }
    }

    u32 SemanticVersion::major() const {
        if (!isValid()) return 0;

        try {
            return std::stoul(m_parts[0]);
        } catch (...) {
            return 0;
        }
    }

    u32 SemanticVersion::minor() const {
        if (!isValid()) return 0;

        try {
            return std::stoul(m_parts[1]);
        } catch (...) {
            return 0;
        }
    }

    u32 SemanticVersion::patch() const {
        if (!isValid()) return 0;

        try {
            return std::stoul(m_parts[2]);
        } catch (...) {
            return 0;
        }
    }

    bool SemanticVersion::nightly() const {
        if (!isValid()) return false;

        return m_parts.size() == 4 && m_parts[3] == "WIP";
    }

    const std::string& SemanticVersion::buildType() const {
        return m_buildType;
    }


    bool SemanticVersion::isValid() const {
        return !m_parts.empty();
    }

    bool SemanticVersion::operator==(const SemanticVersion& other) const {
        return this->m_parts == other.m_parts;
    }

    std::strong_ordering SemanticVersion::operator<=>(const SemanticVersion &other) const {
        if (*this == other)
            return std::strong_ordering::equivalent;

        if (this->major() > other.major())
            return std::strong_ordering::greater;
        if (this->minor() > other.minor())
            return std::strong_ordering::greater;
        if (this->patch() > other.patch())
            return std::strong_ordering::greater;
        if (!this->nightly() && other.nightly())
            return std::strong_ordering::greater;

        return std::strong_ordering::less;
    }

    std::string SemanticVersion::get(bool withBuildType) const {
        if (!isValid()) return "";

        auto result = wolv::util::combineStrings(m_parts, ".");

        if (withBuildType && !m_buildType.empty())
            result += hex::format("-{}", m_buildType);

        return result;
    }


}
