#include <content/helpers/constants.hpp>

#include <nlohmann/json.hpp>
#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    ConstantGroup::ConstantGroup(const std::fs::path &path) {
        if (!wolv::io::fs::exists(path))
            throw std::runtime_error("Path does not exist");

        if (path.extension() != ".json")
            throw std::runtime_error("Invalid constants file extension");

        try {
            auto fileData = wolv::io::File(path, wolv::io::File::Mode::Read).readString();
            auto content = nlohmann::json::parse(fileData);

            m_name = content.at("name").get<std::string>();
            const auto values = content.at("values");
            for (const auto &value : values) {
                Constant constant = {};
                constant.name     = value.at("name").get<std::string>();
                if (value.contains("desc"))
                    constant.description = value.at("desc").get<std::string>();

                constant.value = BinaryPattern(value.at("value").get<std::string>());

                m_constants.push_back(constant);
            }
        } catch (...) {
            throw std::runtime_error("Failed to parse constants file " + wolv::util::toUTF8String(path));
        }
    }

}
