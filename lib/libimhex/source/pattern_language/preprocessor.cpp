#include <hex/pattern_language/preprocessor.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/file.hpp>

#include <filesystem>

namespace hex::pl {

    std::optional<std::string> Preprocessor::preprocess(const std::string &code, bool initialRun) {
        u32 offset      = 0;
        u32 lineNumber  = 1;
        bool isInString = false;

        if (initialRun) {
            this->m_defines.clear();
            this->m_pragmas.clear();
        }

        std::string output;
        output.reserve(code.length());

        try {
            bool startOfLine = true;
            while (offset < code.length()) {
                if (offset > 0 && code[offset - 1] != '\\' && code[offset] == '\"')
                    isInString = !isInString;
                else if (isInString) {
                    output += code[offset];
                    offset += 1;
                    continue;
                }

                if (code[offset] == '#' && startOfLine) {
                    offset += 1;

                    if (code.substr(offset, 7) == "include") {
                        offset += 7;

                        while (std::isblank(code[offset]) || std::isspace(code[offset]))
                            offset += 1;

                        if (code[offset] != '<' && code[offset] != '"')
                            throwPreprocessorError("expected '<' or '\"' before file name", lineNumber);

                        char endChar = code[offset];
                        if (endChar == '<') endChar = '>';

                        offset += 1;

                        std::string includeFile;
                        while (code[offset] != endChar) {
                            includeFile += code[offset];
                            offset += 1;

                            if (offset >= code.length())
                                throwPreprocessorError(hex::format("missing terminating '{0}' character", endChar), lineNumber);
                        }
                        offset += 1;

                        fs::path includePath = includeFile;

                        if (includeFile[0] != '/') {
                            for (const auto &dir : hex::getPath(ImHexPath::PatternsInclude)) {
                                fs::path tempPath = dir / includePath;
                                if (fs::exists(tempPath)) {
                                    includePath = tempPath;
                                    break;
                                }
                            }
                        }

                        File file(includePath, File::Mode::Read);
                        if (!file.isValid()) {
                            if (includePath.parent_path().filename().string() == "std")
                                throwPreprocessorError(hex::format("{0}: No such file or directory.\n\nThis file might be part of the standard library.\nYou can install the standard library though\nthe Content Store found under Help -> Content Store.", includeFile.c_str()), lineNumber);
                            else
                                throwPreprocessorError(hex::format("{0}: No such file or directory", includeFile.c_str()), lineNumber);
                        }

                        bool shouldInclude = true;
                        this->addPragmaHandler("once", [&, includePath, this](const std::string &value) {
                            auto [iter, added] = this->m_onceIncludedFiles.insert(includePath);
                            if (!added) shouldInclude = false;
                            return value.empty();
                        });

                        auto preprocessedInclude = this->preprocess(file.readString(), false);
                        if (!preprocessedInclude.has_value())
                            throw this->m_error;

                        if (shouldInclude) {
                            auto content = preprocessedInclude.value();

                            std::replace(content.begin(), content.end(), '\n', ' ');
                            std::replace(content.begin(), content.end(), '\r', ' ');

                            output += content;
                        }
                    } else if (code.substr(offset, 6) == "define") {
                        offset += 6;

                        while (std::isblank(code[offset])) {
                            offset += 1;
                        }

                        std::string defineName;
                        while (!std::isblank(code[offset])) {
                            defineName += code[offset];

                            if (offset >= code.length() || code[offset] == '\n' || code[offset] == '\r')
                                throwPreprocessorError("no value given in #define directive", lineNumber);
                            offset += 1;
                        }

                        while (std::isblank(code[offset])) {
                            offset += 1;
                            if (offset >= code.length())
                                throwPreprocessorError("no value given in #define directive", lineNumber);
                        }

                        std::string replaceValue;
                        while (code[offset] != '\n' && code[offset] != '\r') {
                            if (offset >= code.length())
                                throwPreprocessorError("missing new line after #define directive", lineNumber);

                            replaceValue += code[offset];
                            offset += 1;
                        }

                        if (replaceValue.empty())
                            throwPreprocessorError("no value given in #define directive", lineNumber);

                        this->m_defines.emplace(defineName, replaceValue, lineNumber);
                    } else if (code.substr(offset, 6) == "pragma") {
                        offset += 6;

                        while (std::isblank(code[offset])) {
                            offset += 1;

                            if (code[offset] == '\n' || code[offset] == '\r')
                                throwPreprocessorError("no instruction given in #pragma directive", lineNumber);
                        }

                        std::string pragmaKey;
                        while (!std::isblank(code[offset]) && code[offset] != '\n' && code[offset] != '\r') {
                            pragmaKey += code[offset];

                            if (offset >= code.length())
                                throwPreprocessorError("no instruction given in #pragma directive", lineNumber);

                            offset += 1;
                        }

                        while (std::isblank(code[offset]))
                            offset += 1;

                        std::string pragmaValue;
                        while (code[offset] != '\n' && code[offset] != '\r') {
                            if (offset >= code.length())
                                throwPreprocessorError("missing new line after #pragma directive", lineNumber);

                            pragmaValue += code[offset];
                            offset += 1;
                        }

                        this->m_pragmas.emplace(pragmaKey, pragmaValue, lineNumber);
                    } else
                        throwPreprocessorError("unknown preprocessor directive", lineNumber);
                } else if (code.substr(offset, 2) == "//") {
                    while (code[offset] != '\n' && offset < code.length())
                        offset += 1;
                } else if (code.substr(offset, 2) == "/*") {
                    while (code.substr(offset, 2) != "*/" && offset < code.length()) {
                        if (code[offset] == '\n') {
                            output += '\n';
                            lineNumber++;
                        }

                        offset += 1;
                    }

                    offset += 2;
                    if (offset >= code.length())
                        throwPreprocessorError("unterminated comment", lineNumber - 1);
                }

                if (code[offset] == '\n') {
                    lineNumber++;
                    startOfLine = true;
                } else if (!std::isspace(code[offset]))
                    startOfLine = false;

                output += code[offset];
                offset += 1;
            }

            if (initialRun) {
                // Apply defines
                std::vector<std::tuple<std::string, std::string, u32>> sortedDefines;
                std::copy(this->m_defines.begin(), this->m_defines.end(), std::back_inserter(sortedDefines));
                std::sort(sortedDefines.begin(), sortedDefines.end(), [](const auto &left, const auto &right) {
                    return std::get<0>(left).size() > std::get<0>(right).size();
                });

                for (const auto &[define, value, defineLine] : sortedDefines) {
                    i32 index = 0;
                    while ((index = output.find(define, index)) != std::string::npos) {
                        output.replace(index, define.length(), value);
                        index += value.length();
                    }
                }

                // Handle pragmas
                for (const auto &[type, value, pragmaLine] : this->m_pragmas) {
                    if (this->m_pragmaHandlers.contains(type)) {
                        if (!this->m_pragmaHandlers[type](value))
                            throwPreprocessorError(hex::format("invalid value provided to '{0}' #pragma directive", type.c_str()), pragmaLine);
                    } else
                        throwPreprocessorError(hex::format("no #pragma handler registered for type {0}", type.c_str()), pragmaLine);
                }
            }
        } catch (PatternLanguageError &e) {
            this->m_error = e;

            return std::nullopt;
        }

        return output;
    }

    void Preprocessor::addPragmaHandler(const std::string &pragmaType, const std::function<bool(const std::string &)> &function) {
        this->m_pragmaHandlers[pragmaType] = function;
    }

    void Preprocessor::addDefaultPragmaHandlers() {
        this->addPragmaHandler("MIME", [](const std::string &value) {
            return !std::all_of(value.begin(), value.end(), isspace) && !value.ends_with('\n') && !value.ends_with('\r');
        });
        this->addPragmaHandler("endian", [](const std::string &value) {
            return value == "big" || value == "little" || value == "native";
        });
        this->addPragmaHandler("once", [](const std::string &value) {
            return value.empty();
        });
    }

}