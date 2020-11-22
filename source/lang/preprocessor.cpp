#include "lang/preprocessor.hpp"

namespace hex::lang {

    Preprocessor::Preprocessor() {

    }

    std::pair<Result, std::string> Preprocessor::preprocess(const std::string& code, bool initialRun) {
        u32 offset = 0;

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
                        return { ResultPreprocessingError, "" };

                    char endChar = code[offset];
                    if (endChar == '<') endChar = '>';

                    offset += 1;

                    std::string includeFile;
                    while (code[offset] != endChar) {
                        includeFile += code[offset];
                        offset += 1;

                        if (offset >= code.length())
                            return { ResultPreprocessingError, "" };
                    }
                    offset += 1;

                    if (includeFile[0] != '/')
                        includeFile = "include/" + includeFile;

                    FILE *file = fopen(includeFile.c_str(), "r");
                    if (file == nullptr)
                        return { ResultPreprocessingError, "" };

                    fseek(file, 0, SEEK_END);
                    size_t size = ftell(file);
                    char *buffer = new char[size + 1];
                    rewind(file);

                    fread(buffer, size, 1, file);
                    buffer[size] = 0x00;

                    auto [result, preprocessedInclude] = this->preprocess(buffer, false);
                    if (result.failed())
                        return { ResultPreprocessingError, "" };

                    output += preprocessedInclude;

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
                            return { ResultPreprocessingError, "" };
                        offset += 1;
                    }

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string replaceValue;
                    while (code[offset] != '\n' && code[offset] != '\r') {
                        if (offset >= code.length())
                            return { ResultPreprocessingError, "" };

                        replaceValue += code[offset];
                        offset += 1;
                    }

                    if (replaceValue.empty())
                        return { ResultPreprocessingError, "" };

                    this->m_defines.emplace(defineName, replaceValue);
                } else if (code.substr(offset, 6) == "pragma") {
                    offset += 6;

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string pragmaKey;
                    while (!std::isblank(code[offset])) {
                        pragmaKey += code[offset];

                        if (offset >= code.length() || code[offset] == '\n' || code[offset] == '\r')
                            return { ResultPreprocessingError, "" };

                        offset += 1;
                    }

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string pragmaValue;
                    while (code[offset] != '\n' && code[offset] != '\r') {
                        if (offset >= code.length())
                            return { ResultPreprocessingError, "" };

                        pragmaValue += code[offset];
                        offset += 1;
                    }

                    if (pragmaValue.empty())
                        return { ResultPreprocessingError, "" };

                    this->m_pragmas.emplace(pragmaKey, pragmaValue);
                } else
                    return { ResultPreprocessingError, "" };
            }

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
                        return { ResultPreprocessingError, { } };
                } else
                    return { ResultPreprocessingError, { } };
            }
        }

        return { ResultSuccess, output };
    }

    void Preprocessor::addPragmaHandler(std::string pragmaType, std::function<bool(std::string)> function) {
        if (!this->m_pragmaHandlers.contains(pragmaType))
            this->m_pragmaHandlers.emplace(pragmaType, function);
    }

    void Preprocessor::addDefaultPragramHandlers() {
        this->addPragmaHandler("MIME", [](std::string value) {
            return !std::all_of(value.begin(), value.end(), isspace) && !value.ends_with('\n') && !value.ends_with('\r');
        });
        this->addPragmaHandler("endian", [](std::string value) {
            return value == "big" || value == "little" || value == "native";
        });
    }

}