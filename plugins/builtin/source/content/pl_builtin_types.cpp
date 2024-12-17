#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/http_requests.hpp>

#include <pl/core/evaluator.hpp>

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>

#include <nlohmann/json.hpp>


namespace hex::plugin::builtin {

    class PatternEncodedString : public pl::ptrn::Pattern {
    public:
        PatternEncodedString(pl::core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternEncodedString(*this));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->getTypeName();
        }
        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }
        void accept(pl::PatternVisitor &v) override {
            v.visit(*this);
        }

        std::vector<pl::u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        void setEncodedString(const EncodingFile &encodingFile, std::span<u8> bytes) {
            m_encodedString.clear();

            u64 offset = 0;
            while (offset < bytes.size()) {
                auto [character, size] = encodingFile.getEncodingFor(std::span(bytes).subspan(offset));
                m_encodedString += std::string(character);

                if (size == 0)
                    break;

                offset += size;
            }
        }

    protected:
        [[nodiscard]] std::string formatDisplayValue() override {
            auto size = std::min<size_t>(this->getSize(), 0x7F);

            if (size == 0)
                return "\"\"";

            std::string buffer(size, 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size, this->getSection());

            return Pattern::callUserFormatFunc(buffer).value_or(fmt::format("\"{0}\" {1}", buffer, size > this->getSize() ? "(truncated)" : ""));
        }

    private:
        std::string m_encodedString;
    };

    namespace {

        std::span<u8> allocateSpace(pl::core::Evaluator *evaluator, const auto &pattern) {
            auto &patternLocalStorage = evaluator->getPatternLocalStorage();
            auto patternLocalAddress = patternLocalStorage.empty() ? 0 : patternLocalStorage.rbegin()->first + 1;
            pattern->setSection(pl::ptrn::Pattern::PatternLocalSectionId);
            pattern->addAttribute("export");

            pattern->setOffset(u64(patternLocalAddress) << 32);
            patternLocalStorage.insert({ patternLocalAddress, { } });

            auto &data = patternLocalStorage[patternLocalAddress].data;
            data.resize(pattern->getSize());

            return data;
        }

        void jsonToPattern(pl::core::Evaluator *evaluator, const nlohmann::json &json, std::vector<std::shared_ptr<pl::ptrn::Pattern>> &entries) {
            u64 index = 0;
            for (auto it = json.begin(); it != json.end(); ++it, ++index) {
                using ValueType = nlohmann::json::value_t;
                switch (it->type()) {
                    case ValueType::object: {
                        auto object = std::make_shared<pl::ptrn::PatternStruct>(evaluator, 0, 0, 0);
                        object->setTypeName("Object");
                        object->setSection(pl::ptrn::Pattern::PatternLocalSectionId);
                        object->addAttribute("export");

                        std::vector<std::shared_ptr<pl::ptrn::Pattern>> objectEntries;
                        jsonToPattern(evaluator, *it, objectEntries);
                        object->setEntries(objectEntries);
                        entries.emplace_back(std::move(object));
                        break;
                    }
                    case ValueType::array: {
                        auto object = std::make_shared<pl::ptrn::PatternArrayDynamic>(evaluator, 0, 0, 0);
                        object->setTypeName("Array");
                        object->setSection(pl::ptrn::Pattern::PatternLocalSectionId);
                        object->addAttribute("export");

                        std::vector<std::shared_ptr<pl::ptrn::Pattern>> objectEntries;
                        jsonToPattern(evaluator, *it, objectEntries);
                        object->setEntries(objectEntries);
                        entries.emplace_back(std::move(object));
                        break;
                    }
                    case ValueType::binary:
                    case ValueType::number_unsigned: {
                        auto object = std::make_shared<pl::ptrn::PatternUnsigned>(evaluator, 0, sizeof(u64), 0);
                        object->setTypeName("u64");

                        auto data = allocateSpace(evaluator, object);
                        auto value = it->get<u64>();
                        std::memcpy(data.data(), &value, sizeof(value));

                        entries.emplace_back(std::move(object));
                        break;
                    }
                    case ValueType::number_integer: {
                        auto object = std::make_shared<pl::ptrn::PatternSigned>(evaluator, 0, sizeof(i64), 0);
                        object->setTypeName("s64");

                        auto data = allocateSpace(evaluator, object);
                        auto value = it->get<i64>();
                        std::memcpy(data.data(), &value, sizeof(value));

                        entries.emplace_back(std::move(object));
                        break;
                    }
                    case ValueType::number_float: {
                        auto object = std::make_shared<pl::ptrn::PatternFloat>(evaluator, 0, sizeof(double), 0);
                        object->setTypeName("double");

                        auto data = allocateSpace(evaluator, object);
                        auto value = it->get<double>();
                        std::memcpy(data.data(), &value, sizeof(value));

                        entries.emplace_back(std::move(object));
                        break;
                    }
                    case ValueType::boolean: {
                        auto object = std::make_shared<pl::ptrn::PatternBoolean>(evaluator, 0, 0);

                        auto data = allocateSpace(evaluator, object);
                        auto value = it->get<bool>();
                        std::memcpy(data.data(), &value, sizeof(value));

                        entries.emplace_back(std::move(object));
                        break;
                    }
                    case ValueType::string: {
                        auto value = it->get<std::string>();

                        auto object = std::make_shared<pl::ptrn::PatternString>(evaluator, 0, value.size(), 0);

                        auto data = allocateSpace(evaluator, object);
                        std::memcpy(data.data(), value.data(), value.size());

                        entries.emplace_back(std::move(object));
                        break;
                    }
                    case ValueType::null:
                    case ValueType::discarded:
                        break;
                }

                auto &lastEntry = entries.back();
                if (json.is_object()) {
                    lastEntry->setVariableName(it.key());
                } else {
                    lastEntry->setArrayIndex(index);
                }
            }
        }


        std::unique_ptr<pl::ptrn::Pattern> jsonToPattern(pl::core::Evaluator *evaluator, auto function) {
            auto object = std::make_unique<pl::ptrn::PatternStruct>(evaluator, 0, 0, 0);
            std::vector<std::shared_ptr<pl::ptrn::Pattern>> patterns;

            try {
                jsonToPattern(evaluator, function(), patterns);
                object->setEntries(patterns);

                return object;
            } catch (const nlohmann::json::exception &e) {
                pl::core::err::E0012.throwError(e.what());
            }
        }

    }

    void registerPatternLanguageTypes() {
        using namespace pl::core;
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        {
            const pl::api::Namespace nsHexDec = { "builtin", "hex", "dec" };

            /* Json<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "Json", FunctionParameterCount::exactly(1), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto data = params[0].toBytes();

                auto result = jsonToPattern(evaluator, [&] { return nlohmann::json::parse(data); });
                result->setSize(data.size());
                return result;
            });

            /* Bson<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "Bson", FunctionParameterCount::exactly(1), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto data = params[0].toBytes();

                auto result = jsonToPattern(evaluator, [&] { return nlohmann::json::from_bson(data); });
                result->setSize(data.size());
                return result;
            });

            /* Cbor<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "Cbor", FunctionParameterCount::exactly(1), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto data = params[0].toBytes();

                auto result = jsonToPattern(evaluator, [&] { return nlohmann::json::from_cbor(data); });
                result->setSize(data.size());
                return result;
            });

            /* Bjdata<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "Bjdata", FunctionParameterCount::exactly(1), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto data = params[0].toBytes();

                auto result = jsonToPattern(evaluator, [&] { return nlohmann::json::from_bjdata(data); });
                result->setSize(data.size());
                return result;
            });

            /* Msgpack<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "Msgpack", FunctionParameterCount::exactly(1), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto data = params[0].toBytes();

                auto result = jsonToPattern(evaluator, [&] { return nlohmann::json::from_msgpack(data); });
                result->setSize(data.size());
                return result;
            });

            /* Ubjson<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "Ubjson", FunctionParameterCount::exactly(1), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto data = params[0].toBytes();

                auto result = jsonToPattern(evaluator, [&] { return nlohmann::json::from_ubjson(data); });
                result->setSize(data.size());
                return result;
            });


            /* EncodedString<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "EncodedString", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto bytes = params[0].toBytes();
                auto encodingDefinition = params[1].toString();

                std::string value;
                EncodingFile encodingFile(EncodingFile::Type::Thingy, encodingDefinition);

                auto pattern = std::make_unique<PatternEncodedString>(evaluator, evaluator->getReadOffset(), bytes.size(), 0);
                pattern->setEncodedString(encodingFile, bytes);

                return pattern;
            });
        }
    }
}
