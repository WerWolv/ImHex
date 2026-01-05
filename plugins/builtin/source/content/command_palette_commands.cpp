#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/content_registry/command_palette.hpp>
#include <hex/api/content_registry/tools.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/views.hpp>

#include <hex/ui/view.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/providers/provider.hpp>
#include <toasts/toast_notification.hpp>

#include <wolv/math_eval/math_evaluator.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    namespace {

        class Value {
        public:
            enum class Unit: u8 {
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
                        value.resize(index);
                    }
                } else {
                    m_unit = Unit::Unitless;
                }


                try {
                    if (!value.contains('.')) {
                        m_value = i128(double(std::stoull(value, nullptr, 0) * static_cast<long double>(m_multiplier)));
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

            std::string formatAs(const Value &other) {
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
                                        return fmt::format("{0}", i64(value / multipler));
                                    else
                                        return fmt::format("{0:.3f}", double(value / multipler));
                                case Unit::Hexadecimal:
                                    return fmt::format("0x{0:x}", u64(value / multipler));
                                case Unit::Binary:
                                    return fmt::format("0b{0:b}", u64(value / multipler));
                                case Unit::Octal:
                                    return fmt::format("0o{0:o}", u64(value / multipler));
                                case Unit::Bytes:
                                    return fmt::format("{0}", u64(value / multipler));
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
                                        return fmt::format("{0}", i64(value / multipler));
                                    else
                                        return fmt::format("{0:.3f}", double(value / multipler));
                                case Unit::Hexadecimal:
                                    return fmt::format("0x{0:x}", u64(value / multipler));
                                case Unit::Binary:
                                    return fmt::format("0b{0:b}", u64(value / multipler));
                                case Unit::Octal:
                                    return fmt::format("0o{0:o}", u64(value / multipler));
                                case Unit::Bytes:
                                    return fmt::format("{0}", u64((value / multipler) / 8));
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
                                        return fmt::format("{0}", i64(value / multipler));
                                    else
                                        return fmt::format("{0:.3f}", double(value / multipler));
                                case Unit::Hexadecimal:
                                    return fmt::format("0x{0:x}", u64(value / multipler));
                                case Unit::Binary:
                                    return fmt::format("0b{0:b}", u64(value / multipler));
                                case Unit::Octal:
                                    return fmt::format("0o{0:o}", u64(value / multipler));
                                case Unit::Bits:
                                    return fmt::format("{0}", u64((value / multipler) * 8));
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

        ContentRegistry::CommandPalette::add(
            ContentRegistry::CommandPalette::Type::SymbolCommand,
            "=",
            "hex.builtin.command.calc.desc",
            [](auto input) {
                wolv::math_eval::MathEvaluator<long double> evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                std::optional<long double> result = evaluator.evaluate(input);
                if (result.has_value())
                    return fmt::format("{0} = {1}", input.data(), result.value());
                else if (evaluator.hasError())
                    return fmt::format("Error: {}", *evaluator.getLastError());
                else
                    return std::string("???");
            }, [](auto input) -> std::optional<std::string> {
                wolv::math_eval::MathEvaluator<long double> evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                std::optional<long double> result = evaluator.evaluate(input);
                if (result.has_value()) {
                    return fmt::format("= {}", result.value());
                } else {
                    return std::nullopt;
                }
            });

        ContentRegistry::CommandPalette::add(
            ContentRegistry::CommandPalette::Type::SymbolCommand,
            "@",
            "hex.builtin.command.goto.desc",
            [](auto input) {
                wolv::math_eval::MathEvaluator<i64> evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                const auto result = evaluator.evaluate(input);
                if (result.has_value())
                    return fmt::format("hex.builtin.command.goto.result"_lang, static_cast<u64>(result.value()));
                else if (evaluator.hasError())
                    return fmt::format("Error: {}", *evaluator.getLastError());
                else
                    return std::string("???");
            }, [](auto input) -> std::optional<std::string> {
                wolv::math_eval::MathEvaluator<i64> evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                const auto result = evaluator.evaluate(input);
                if (result.has_value()) {
                    ImHexApi::HexEditor::setSelection(result.value(), 1);
                }
                return std::nullopt;
            });

        ContentRegistry::CommandPalette::add(
            ContentRegistry::CommandPalette::Type::KeywordCommand,
            "/web",
            "hex.builtin.command.web.desc",
            [](auto input) {
                return fmt::format("hex.builtin.command.web.result"_lang, input.data());
            },
            [](auto input) {
                hex::openWebpage(input);
                return std::nullopt;
            });

        ContentRegistry::CommandPalette::add(
            ContentRegistry::CommandPalette::Type::SymbolCommand,
            "$",
            "hex.builtin.command.cmd.desc",
            [](auto input) {
                return fmt::format("hex.builtin.command.cmd.result"_lang, input.data());
            },
            [](auto input) {
                if (input.starts_with("imhex ")) {
                    // Handle ImHex internal commands
                    auto command = input.substr(6);
                    auto parts = splitConversionCommandInput(command);
                    if (!parts.empty()) {
                        if (parts.size() == 2 && parts[0] == "debug") {
                            if (parts[1] == "on") {
                                ContentRegistry::Settings::write<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.debug_mode_enabled", true);
                                ui::ToastInfo::open("Debug mode enabled!");
                            } else if (parts[1] == "off") {
                                ContentRegistry::Settings::write<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.debug_mode_enabled", false);
                                ui::ToastInfo::open("Debug mode disabled!");
                            }
                        }
                    }
                } else {
                    hex::executeCommand(input);
                }

                return std::nullopt;
            });

        ContentRegistry::CommandPalette::addHandler(
                ContentRegistry::CommandPalette::Type::SymbolCommand,
                ">",
                [](const auto &input) {
                    std::vector<ContentRegistry::CommandPalette::impl::QueryResult> result;

                    for (const auto &[priority, entry] : ContentRegistry::UserInterface::impl::getMenuItems()) {
                        if (!entry.enabledCallback())
                            continue;

                        if (entry.view != nullptr) {
                            if (View::getLastFocusedView() != entry.view)
                                continue;
                        }

                        std::vector<std::string> names;
                        std::transform(entry.unlocalizedNames.begin(), entry.unlocalizedNames.end(), std::back_inserter(names), [](auto &name) { return Lang(name); });

                        if (auto combined = wolv::util::combineStrings(names, " -> "); hex::containsIgnoreCase(combined, input) && !combined.contains(ContentRegistry::UserInterface::impl::SeparatorValue) && !combined.contains(ContentRegistry::UserInterface::impl::SubMenuValue)) {
                            result.emplace_back(ContentRegistry::CommandPalette::impl::QueryResult {
                                std::move(combined),
                                [&entry](const auto&) { entry.callback(); }
                            });
                        }
                    }

                    return result;
                },
                [](auto input) {
                    return fmt::format("Menu Item: {}", input.data());
                });

        ContentRegistry::CommandPalette::addHandler(
        ContentRegistry::CommandPalette::Type::SymbolCommand,
        ".",
        [](const auto &input) {
            std::vector<ContentRegistry::CommandPalette::impl::QueryResult> result;

            u32 index = 0;
            for (const auto &provider : ImHexApi::Provider::getProviders()) {
                ON_SCOPE_EXIT { index += 1; };

                auto name = provider->getName();
                if (!hex::containsIgnoreCase(name, input))
                    continue;

                result.emplace_back(ContentRegistry::CommandPalette::impl::QueryResult {
                    provider->getName(),
                    [index](const auto&) { ImHexApi::Provider::setCurrentProvider(index); }
                });
            }

            return result;
        },
        [](auto input) {
            return fmt::format("Data Source: {}", input.data());
        });

        ContentRegistry::CommandPalette::add(
                    ContentRegistry::CommandPalette::Type::SymbolCommand,
                    "%",
                    "hex.builtin.command.convert.desc",
                    handleConversionCommand);

        ContentRegistry::CommandPalette::addHandler(
                ContentRegistry::CommandPalette::Type::SymbolCommand,
                "+",
                [](const auto &input) {
                    std::vector<ContentRegistry::CommandPalette::impl::QueryResult> result;

                    for (const auto &[unlocalizedName, view] : ContentRegistry::Views::impl::getEntries()) {
                        if (!view->shouldProcess())
                            continue;
                        if (!view->hasViewMenuItemEntry())
                            continue;

                        const auto name = Lang(unlocalizedName);
                        if (!hex::containsIgnoreCase(name, input))
                            continue;
                        result.emplace_back(fmt::format("Focus {} View", name), [&view](const auto &) {
                            view->bringToFront();
                        });
                    }

                    return result;
                },
                [](auto input) {
                    return fmt::format("Focus {} View", input.data());
                });

        ContentRegistry::CommandPalette::addHandler(
            ContentRegistry::CommandPalette::Type::KeywordCommand,
            "/tool",
            [](const auto &input) {
                std::vector<ContentRegistry::CommandPalette::impl::QueryResult> result;

                for (const auto &toolEntry : ContentRegistry::Tools::impl::getEntries()) {
                    const auto name = Lang(toolEntry.unlocalizedName);
                    if (!hex::containsIgnoreCase(name, input) && !std::string("/tool").contains(input))
                        continue;

                    result.emplace_back(name, [&toolEntry](const auto &) {
                        ContentRegistry::CommandPalette::setDisplayedContent([&toolEntry]() {
                            toolEntry.function();
                        });
                    });
                }

                return result;
            },
            [](const auto &input) { return input; });
    }

}