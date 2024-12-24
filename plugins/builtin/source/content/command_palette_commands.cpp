#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/api/shortcut_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/math_eval/math_evaluator.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    namespace {

        class Value {
        public:
            enum class Unit {
                Unitless,
                Decimal,
                Hexadecimal,
                Binary,
                Octal,
                Bits,
                Bytes,
                Invalid
            };

            explicit Value(std::string value) {
                if (!value.starts_with("0x") && !value.starts_with("0b")) {
                    auto index = value.find_first_not_of("0123456789.,");
                    if (index == std::string::npos) {
                        m_unit = Unit::Unitless;
                    } else {
                        std::tie(m_unit, m_multiplier) = parseUnit(value.substr(index));
                        value = value.substr(0, index);
                    }
                } else {
                    m_unit = Unit::Unitless;
                }


                try {
                    if (!value.contains('.')) {
                        m_value = i128(std::stoull(value, nullptr, 0) * static_cast<long double>(m_multiplier));
                    } else {
                        m_value = std::stod(value) * m_multiplier;
                    }
                } catch (const std::exception &) {
                    m_value = i128(0);
                    m_unit = Unit::Invalid;
                    m_unitString.clear();
                    m_multiplier = 1;
                }
            }

            std::string formatAs(Value other) {
                return std::visit([&, this]<typename T>(T value) -> std::string {

                    auto unit = other.getUnit();
                    auto multipler = other.getMultiplier();

                    bool isInteger = std::integral<T> && multipler == 1;
                    switch (m_unit) {
                        case Unit::Invalid:
                            if (this->getUnitString() != other.getUnitString())
                                return "hex.builtin.command.convert.invalid_conversion"_lang;
                            unit = Unit::Decimal;
                            [[fallthrough]];
                        case Unit::Unitless: {

                            switch (unit) {
                                case Unit::Unitless:
                                case Unit::Decimal:
                                    if (isInteger)
                                        return hex::format("{0}", value / multipler);
                                    else
                                        return hex::format("{0:.3f}", value / multipler);
                                case Unit::Hexadecimal:
                                    return hex::format("0x{0:x}", u128(value / multipler));
                                case Unit::Binary:
                                    return hex::format("0b{0:b}", u128(value / multipler));
                                case Unit::Octal:
                                    return hex::format("0o{0:o}", u128(value / multipler));
                                case Unit::Bytes:
                                    return hex::format("{0}", u128(value / multipler));
                                default:
                                    return "hex.builtin.command.convert.invalid_conversion"_lang;
                            }

                            break;
                        }
                        case Unit::Bits: {

                            switch (unit) {
                                case Unit::Bits:
                                case Unit::Decimal:
                                    if (isInteger)
                                        return hex::format("{0}", value / multipler);
                                    else
                                        return hex::format("{0:.3f}", value / multipler);
                                case Unit::Hexadecimal:
                                    return hex::format("0x{0:x}", u128(value / multipler));
                                case Unit::Binary:
                                    return hex::format("0b{0:b}", u128(value / multipler));
                                case Unit::Octal:
                                    return hex::format("0o{0:o}", u128(value / multipler));
                                case Unit::Bytes:
                                    return hex::format("{0}", u128((value / multipler) / 8));
                                default:
                                    return "hex.builtin.command.convert.invalid_conversion"_lang;
                            }

                            break;
                        }
                        case Unit::Bytes: {

                            switch (unit) {
                                case Unit::Bytes:
                                case Unit::Decimal:
                                    if (isInteger)
                                        return hex::format("{0}", value / multipler);
                                    else
                                        return hex::format("{0:.3f}", value / multipler);
                                case Unit::Hexadecimal:
                                    return hex::format("0x{0:x}", u128(value / multipler));
                                case Unit::Binary:
                                    return hex::format("0b{0:b}", u128(value / multipler));
                                case Unit::Octal:
                                    return hex::format("0o{0:o}", u128(value / multipler));
                                case Unit::Bits:
                                    return hex::format("{0}", u128((value / multipler) * 8));
                                default:
                                    return "hex.builtin.command.convert.invalid_conversion"_lang;
                            }

                            break;
                        }
                        default:
                            return "hex.builtin.command.convert.invalid_input"_lang;
                    }
                }, m_value);
            }

            [[nodiscard]] Unit getUnit() const { return m_unit; }
            [[nodiscard]] double getMultiplier() const { return m_multiplier; }
            [[nodiscard]] const std::string& getUnitString() const { return m_unitString; }

        private:
            std::pair<Unit, double> parseUnit(std::string unitString, bool parseMultiplier = true) {
                auto unitStringCopy = unitString;
                unitString = wolv::util::trim(unitString);

                double multiplier = 1;
                if (parseMultiplier && !unitString.starts_with("dec") && !unitString.starts_with("hex") && !unitString.starts_with("bin") && !unitString.starts_with("oct")) {
                    if (unitString.starts_with("Ki"))      { multiplier = 1024ULL;                                          unitString = unitString.substr(2); }
                    else if (unitString.starts_with("Mi")) { multiplier = 1024ULL * 1024ULL;                                unitString = unitString.substr(2); }
                    else if (unitString.starts_with("Gi")) { multiplier = 1024ULL * 1024ULL * 1024ULL;                      unitString = unitString.substr(2); }
                    else if (unitString.starts_with("Ti")) { multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;            unitString = unitString.substr(2); }
                    else if (unitString.starts_with("Pi")) { multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;  unitString = unitString.substr(2); }
                    else if (unitString.starts_with("k"))  { multiplier = 1E3;   unitString = unitString.substr(1); }
                    else if (unitString.starts_with("M"))  { multiplier = 1E6;   unitString = unitString.substr(1); }
                    else if (unitString.starts_with("G"))  { multiplier = 1E9;   unitString = unitString.substr(1); }
                    else if (unitString.starts_with("T"))  { multiplier = 1E12;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("P"))  { multiplier = 1E15;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("E"))  { multiplier = 1E18;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("Z"))  { multiplier = 1E21;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("Y"))  { multiplier = 1E24;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("d"))  { multiplier = 1E-1;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("c"))  { multiplier = 1E-2;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("m"))  { multiplier = 1E-3;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("u"))  { multiplier = 1E-6;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("n"))  { multiplier = 1E-9;  unitString = unitString.substr(1); }
                    else if (unitString.starts_with("p"))  { multiplier = 1E-12; unitString = unitString.substr(1); }
                    else if (unitString.starts_with("f"))  { multiplier = 1E-15; unitString = unitString.substr(1); }
                    else if (unitString.starts_with("a"))  { multiplier = 1E-18; unitString = unitString.substr(1); }
                    else if (unitString.starts_with("z"))  { multiplier = 1E-21; unitString = unitString.substr(1); }
                    else if (unitString.starts_with("y"))  { multiplier = 1E-24; unitString = unitString.substr(1); }
                    else { return parseUnit(unitString, false); }
                }

                unitString = wolv::util::trim(unitString);
                m_unitString = unitString;
                if (unitString.empty()) {
                    if (multiplier == 1) {
                        return { Unit::Unitless, 1 };
                    } else {
                        m_unitString = unitStringCopy;
                        return { Unit::Unitless, 1 };
                    }

                } else if (unitString == "bit" || unitString == "bits" || unitString == "b") {
                    return { Unit::Bits, multiplier };
                } else if (unitString == "byte" || unitString == "bytes" || unitString == "B") {
                    return { Unit::Bytes, multiplier };
                } else if (unitString == "hex" || unitString == "hex.builtin.command.convert.hexadecimal"_lang.get()) {
                    return { Unit::Hexadecimal, multiplier };
                } else if (unitString == "bin" || unitString == "hex.builtin.command.convert.binary"_lang.get()) {
                    return { Unit::Binary, multiplier };
                } else if (unitString == "oct" || unitString == "hex.builtin.command.convert.octal"_lang.get()) {
                    return { Unit::Octal, multiplier };
                } else if (unitString == "dec" || unitString == "hex.builtin.command.convert.decimal"_lang.get()) {
                    return { Unit::Decimal, multiplier };
                } else {
                    return { Unit::Invalid, multiplier };
                }
            }

        private:
            Unit m_unit;
            std::string m_unitString;
            double m_multiplier = 1;
            std::variant<i128, double> m_value;
        };

        std::vector<std::string> splitConversionCommandInput(const std::string &input) {
            std::vector<std::string> parts = wolv::util::splitString(input, " ", true);
            std::erase_if(parts, [](auto &part) { return part.empty(); });

            return parts;
        }

        bool verifyConversionInput(const std::vector<std::string> &parts) {
            if (parts.size() == 3) {
                return parts[1] == "hex.builtin.command.convert.to"_lang.get() || parts[1] == "hex.builtin.command.convert.in"_lang.get() || parts[1] == "hex.builtin.command.convert.as"_lang.get();
            } else {
                return false;
            }
        }


        std::string handleConversionCommand(const std::string &input) {
            auto parts = splitConversionCommandInput(input);

            if (!verifyConversionInput(parts))
                return "hex.builtin.command.convert.invalid_input"_lang;

            auto from = Value(parts[0]);
            auto to = Value("1" + parts[2]);

            return fmt::format("% {}", from.formatAs(to));
        }

    }

    void registerCommandPaletteCommands() {

        ContentRegistry::CommandPaletteCommands::add(
            ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
            "=",
            "hex.builtin.command.calc.desc",
            [](auto input) {
                wolv::math_eval::MathEvaluator<long double> evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                std::optional<long double> result = evaluator.evaluate(input);
                if (result.has_value())
                    return hex::format("{0} = {1}", input.data(), result.value());
                else if (evaluator.hasError())
                    return hex::format("Error: {}", *evaluator.getLastError());
                else
                    return std::string("???");
            }, [](auto input) -> std::optional<std::string> {
                wolv::math_eval::MathEvaluator<long double> evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                std::optional<long double> result = evaluator.evaluate(input);
                if (result.has_value()) {
                    return hex::format("= {}", result.value());
                } else {
                    return std::nullopt;
                }
            });

        ContentRegistry::CommandPaletteCommands::add(
            ContentRegistry::CommandPaletteCommands::Type::KeywordCommand,
            "/web",
            "hex.builtin.command.web.desc",
            [](auto input) {
                return hex::format("hex.builtin.command.web.result"_lang, input.data());
            },
            [](auto input) {
                hex::openWebpage(input);
                return std::nullopt;
            });

        ContentRegistry::CommandPaletteCommands::add(
            ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
            "$",
            "hex.builtin.command.cmd.desc",
            [](auto input) {
                return hex::format("hex.builtin.command.cmd.result"_lang, input.data());
            },
            [](auto input) {
                hex::executeCommand(input);
                return std::nullopt;
            });

        ContentRegistry::CommandPaletteCommands::addHandler(
                ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
                ">",
                [](const auto &input) {
                    std::vector<ContentRegistry::CommandPaletteCommands::impl::QueryResult> result;

                    for (const auto &[priority, entry] : ContentRegistry::Interface::impl::getMenuItems()) {
                        if (!entry.enabledCallback())
                            continue;

                        std::vector<std::string> names;
                        std::transform(entry.unlocalizedNames.begin(), entry.unlocalizedNames.end(), std::back_inserter(names), [](auto &name) { return Lang(name); });

                        if (auto combined = wolv::util::combineStrings(names, " -> "); hex::containsIgnoreCase(combined, input) && !combined.contains(ContentRegistry::Interface::impl::SeparatorValue) && !combined.contains(ContentRegistry::Interface::impl::SubMenuValue)) {
                            result.emplace_back(ContentRegistry::CommandPaletteCommands::impl::QueryResult {
                                std::move(combined),
                                [&entry](const auto&) { entry.callback(); }
                            });
                        }
                    }

                    return result;
                },
                [](auto input) {
                    return hex::format("Menu Item: {}", input.data());
                });

        ContentRegistry::CommandPaletteCommands::addHandler(
        ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
        ".",
        [](const auto &input) {
            std::vector<ContentRegistry::CommandPaletteCommands::impl::QueryResult> result;

            u32 index = 0;
            for (const auto &provider : ImHexApi::Provider::getProviders()) {
                ON_SCOPE_EXIT { index += 1; };

                auto name = provider->getName();
                if (!hex::containsIgnoreCase(name, input))
                    continue;

                result.emplace_back(ContentRegistry::CommandPaletteCommands::impl::QueryResult {
                    provider->getName(),
                    [index](const auto&) { ImHexApi::Provider::setCurrentProvider(index); }
                });
            }

            return result;
        },
        [](auto input) {
            return hex::format("Provider: {}", input.data());
        });

        ContentRegistry::CommandPaletteCommands::add(
                    ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
                    "%",
                    "hex.builtin.command.convert.desc",
                    handleConversionCommand);

    }

}