#include "lang/preprocessor.hpp"

namespace hex::lang {

    Preprocessor::Preprocessor() {

    }

    std::pair<Result, std::string> Preprocessor::preprocess(const std::string& code, bool applyDefines) {
        u32 offset = 0;

        if (applyDefines)
            this->m_defines.clear();

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
                        offset += 1;

                        if (offset >= code.length() || code[offset] == '\n' || code[offset] == '\r')
                            return { ResultPreprocessingError, "" };
                    }

                    while (std::isblank(code[offset]))
                        offset += 1;

                    std::string replaceValue;
                    do {
                        if (offset >= code.length())
                            return { ResultPreprocessingError, "" };

                        replaceValue += code[offset];
                        offset += 1;
                    } while (code[offset] != '\n' && code[offset] != '\r');

                    this->m_defines.emplace(defineName, replaceValue);
                }
            }

            output += code[offset];
            offset += 1;
        }

        if (applyDefines) {
            for (const auto &[define, value] : this->m_defines) {
                s32 index = 0;
                while((index = output.find(define, index)) != std::string::npos) {
                    if (index > 0) {
                        output.replace(index, define.length(), value);
                        index += value.length();
                    }
                }
            }
        }

        return { ResultSuccess, output };
    }

}