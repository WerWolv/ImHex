#include "lang/preprocessor.hpp"

namespace hex::lang {

    Preprocessor::Preprocessor() {

    }

    std::optional<std::string> Preprocessor::preprocess(const std::string& code, bool initialRun) {
        u32 offset = 0;
        u32 lineNumber = 1;

        if (initialRun) {
            this->m_defines.clear();
            this->m_pragmas.clear();
        }

        std::string output;
        output.reserve(code.length());

        while (offset < code.length()) {
            if (code[offset] == '#') {
                offset += 1;

                if (code.substr(offset, 7) == "include") {
                    offset += 7;

                    while (std::isblank(code[offset]) || std::isspace(code[offset]))
                        offset += 1;

                    if (code[offset] != '<' && code[offset] != '"')
                        return { };

                    char endChar = code[offset];
                    if (endChar == '<') endChar = '>';

                    offset += 1;

                    std::string includeFile;
                    while (code[offset] != endChar) {
                        includeFile += code[offset];
                        offset += 1;

                        if (offset >= code.length())
                            return { };
                    }
                    offset += 1;

                    if (includeFile[0] != '/')
                        includeFile = "include/" + includeFile;

                    FILE *file = fopen(includeFile.c_str(), "r");
                    if (file == nullptr)
                        return { };

                    fseek(file, 0, SEEK_END);
                    size_t size = ftell(file);
                    char *buffer = new char[size + 1];
                    rewind(file);

                    fread(buffer, size, 1, file);
                    buffer[size] = 0x00;

                    auto preprocessedInclude = this->preprocess(buffer, false);
                    if (!preprocessedInclude.has_value())
                        return { };

                    output += preprocessedInclude.value();

                    delete[] buffer;

                    fclose(file);
                } else if (code.substr(offset, 6) == "define") {
                    offset += 6;

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string defineName;
                    while (!std::isblank(code[offset])) {
                        defineName += code[offset];

                        if (offset >= code.length() || code[offset] == '\n' || code[offset] == '\r')
                            return { };
                        offset += 1;
                    }

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string replaceValue;
                    while (code[offset] != '\n' && code[offset] != '\r') {
                        if (offset >= code.length())
                            return { };

                        replaceValue += code[offset];
                        offset += 1;
                    }

                    if (replaceValue.empty())
                        return { };

                    this->m_defines.emplace(defineName, replaceValue);
                } else if (code.substr(offset, 6) == "pragma") {
                    offset += 6;

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string pragmaKey;
                    while (!std::isblank(code[offset])) {
                        pragmaKey += code[offset];

                        if (offset >= code.length() || code[offset] == '\n' || code[offset] == '\r')
                            return { };

                        offset += 1;
                    }

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string pragmaValue;
                    while (code[offset] != '\n' && code[offset] != '\r') {
                        if (offset >= code.length())
                            return { };

                        pragmaValue += code[offset];
                        offset += 1;
                    }

                    if (pragmaValue.empty())
                        return { };

                    this->m_pragmas.emplace(pragmaKey, pragmaValue);
                } else
                    return { };
            }

            if (code[offset] == '\n')
                lineNumber++;

            output += code[offset];
            offset += 1;
        }

        if (initialRun) {
            // Apply defines
            for (const auto &[define, value] : this->m_defines) {
                s32 index = 0;
                while((index = output.find(define, index)) != std::string::npos) {
                    output.replace(index, define.length(), value);
                    index += value.length();
                }
            }

            // Handle pragmas
            for (const auto &[type, value] : this->m_pragmas) {
                if (this->m_pragmaHandlers.contains(type)) {
                    if (!this->m_pragmaHandlers[type](value))
                        return { };
                } else
                    return { };
            }
        }

        return output;
    }

    void Preprocessor::addPragmaHandler(std::string pragmaType, std::function<bool(std::string)> function) {
        if (!this->m_pragmaHandlers.contains(pragmaType))
            this->m_pragmaHandlers.emplace(pragmaType, function);
    }

    void Preprocessor::addDefaultPragmaHandlers() {
        this->addPragmaHandler("MIME", [](std::string value) {
            return !std::all_of(value.begin(), value.end(), isspace) && !value.ends_with('\n') && !value.ends_with('\r');
        });
        this->addPragmaHandler("endian", [](std::string value) {
            return value == "big" || value == "little" || value == "native";
        });
    }

}