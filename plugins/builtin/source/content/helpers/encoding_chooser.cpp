#include <content/helpers/encoding_chooser.hpp>

#include <hex/helpers/utils.hpp>

#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    EncodingChoices readEncodingChoices(const std::vector<std::fs::path> &paths) {
        std::map<std::fs::path, EncodingHeader> headers;
        std::map<std::fs::path, std::vector<std::string>> aliases;

        for (const auto &path : paths) {
            if (path.extension() != ".tbl")
                continue;

            auto header = readEncodingHeader(path);
            if (!header.isAlias) {
                headers[path] = std::move(header);
                continue;
            }

            // A link that breaks or loops reaches no table, so it has nothing to give a name to.
            if (!header.path.empty())
                aliases[header.path].push_back(path.stem().string());
        }

        EncodingChoices result;

        for (const auto &path : paths) {
            if (path.extension() != ".tbl") {
                result.files.push_back(path);
                continue;
            }

            const auto header = headers.find(path);
            if (header == headers.end())
                continue;

            result.files.push_back(path);

            auto &entry = result.entries[path];
            entry.name = header->second.name;
            entry.description = header->second.description;

            if (const auto alias = aliases.find(path); alias != aliases.end())
                entry.aliases = alias->second;
        }

        return result;
    }

    EncodingChoiceLines getEncodingChoiceLines(const EncodingChoice &choice, const std::string &fileName) {
        const auto &name = choice.name.empty() ? fileName : choice.name;

        // The file's name only says something the encoding's name does not already say.
        std::vector<std::string> otherNames;
        if (toLower(name) != toLower(fileName))
            otherNames.push_back(fileName);

        otherNames.insert(otherNames.end(), choice.aliases.begin(), choice.aliases.end());
        const auto joined = wolv::util::combineStrings(otherNames, ", ");

        if (choice.description.empty())
            return { name, joined };

        return { choice.description, otherNames.empty() ? name : name + " (" + joined + ")" };
    }

    std::string getEncodingFileName(const std::fs::path &adjustedPath) {
        return wolv::util::toUTF8String(std::fs::path(adjustedPath).replace_extension(""));
    }

}
