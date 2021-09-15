#include "views/view_help.hpp"
#include <hex/helpers/paths.hpp>

#include <imgui_imhex_extensions.h>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex {

    ViewHelp::ViewHelp() : View("hex.view.help.about.name") {
    }

    ViewHelp::~ViewHelp() {

    }


    static void drawTitle(const std::string &title) {
        ImGui::TextColored(ImVec4(0.6F, 0.6F, 1.0F, 1.0F), "%s", title.c_str());
    }

    static void drawBuiltinFunction(
        const std::string &return_type,
        const std::string &name,
        const std::string &arguments,
        const std::string &description
    ) {
        ImGui::Bullet();
        ImGui::TextColored(ImVec4(0.3F, 0.7F, 0.2F, 1.0F), "%s", return_type.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.57F, 0.24F, 0.69F, 1.0F), "%s", name.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.71F, 0.19F, 0.31F, 1.0F), "(");
        ImGui::SameLine();
        ImGui::Text("%s", arguments.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.71F, 0.19F, 0.31F, 1.0F), ")");
        ImGui::SameLine();
        ImGui::TextWrapped(" - %s", description.c_str());
    }

    static void drawCodeSegment(const std::string &id, const std::string &code) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2F, 0.2F, 0.2F, 0.3F));
        if (ImGui::BeginChild(id.c_str(), ImVec2(-1, ImGui::CalcTextSize(code.c_str()).y))) {
            ImGui::Text("%s", code.c_str());
        }
        ImGui::EndChild();

        ImGui::NewLine();
        ImGui::PopStyleColor();
    };

    static void drawUnaryBulletPoint(const std::string &op, const std::string &name) {
        ImGui::Bullet();
        ImGui::TextWrapped("a %s b - %s", op.c_str(), name.c_str());
    }

    void ViewHelp::drawAboutPopup() {
        if (ImGui::BeginPopupModal(View::toWindowName("hex.view.help.about.name").c_str(), &this->m_aboutWindowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("ImHex Hex Editor v%s by WerWolv - " ICON_FA_CODE_BRANCH, IMHEX_VERSION);
            #if defined(GIT_BRANCH) && defined(GIT_COMMIT_HASH)
                ImGui::SameLine();
                if (ImGui::Hyperlink(hex::format("{0}@{1}", GIT_BRANCH, GIT_COMMIT_HASH).c_str()))
                    hex::openWebpage("https://github.com/WerWolv/ImHex/commit/" GIT_COMMIT_HASH);
            #endif
            ImGui::TextUnformatted("hex.view.help.about.translator"_lang);


            ImGui::TextUnformatted("hex.view.help.about.source"_lang); ImGui::SameLine();
            if (ImGui::Hyperlink("WerWolv/ImHex"))
                hex::openWebpage("https://github.com/WerWolv/ImHex");
            ImGui::NewLine();

            ImGui::TextUnformatted("hex.view.help.about.donations"_lang);
            ImGui::Separator();

            constexpr const char* Links[] = { "https://werwolv.net/donate", "https://www.patreon.com/werwolv", "https://github.com/sponsors/WerWolv" };

            ImGui::TextWrapped("%s", static_cast<const char *>("hex.view.help.about.thanks"_lang));

            ImGui::NewLine();

            for (auto &link : Links) {
                if (ImGui::Hyperlink(link))
                    hex::openWebpage(link);
            }
            ImGui::NewLine();

            const auto Link = [](const std::string &label, const std::string &url) {
                if (ImGui::BulletHyperlink(label.data()))
                    hex::openWebpage(url.data());
            };

            ImGui::TextUnformatted("hex.view.help.about.libs"_lang);
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2F, 0.2F, 0.2F, 0.3F));
            Link("ImGui by ocornut", "https://github.com/ocornut/imgui");
            Link("imgui_club by ocornut", "https://github.com/ocornut/imgui_club");
            Link("imnodes by Nelarius", "https://github.com/Nelarius/imnodes");
            Link("ImGuiColorTextEdit by BalazsJako", "https://github.com/BalazsJako/ImGuiColorTextEdit");
            Link("ImPlot by epezent", "https://github.com/epezent/implot");
            Link("capstone by aquynh", "https://github.com/aquynh/capstone");
            Link("JSON for Modern C++ by nlohmann", "https://github.com/nlohmann/json");
            Link("YARA by VirusTotal", "https://github.com/VirusTotal/yara");
            Link("Native File Dialog Extended by btzy and mlabbe", "https://github.com/btzy/nativefiledialog-extended");
            ImGui::NewLine();
            Link("GNU libmagic", "http://www.darwinsys.com/file/");
            Link("GLFW3", "https://github.com/glfw/glfw");
            Link("LLVM", "https://github.com/llvm/llvm-project");
            Link("Python 3", "https://github.com/python/cpython");
            Link("FreeType", "https://gitlab.freedesktop.org/freetype/freetype");
            Link("Mbed TLS", "https://github.com/ARMmbed/mbedtls");

            ImGui::PopStyleColor();

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::NewLine();
            ImGui::TextUnformatted("hex.view.help.about.paths"_lang);
            ImGui::Separator();

            if (ImGui::BeginTable("##imhex_paths", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Paths");

                constexpr std::array<std::pair<const char*, ImHexPath>, 8> PathTypes = {{
                    { "Resources", ImHexPath::Resources },
                    { "Config", ImHexPath::Config },
                    { "Magic", ImHexPath::Magic },
                    { "Patterns", ImHexPath::Patterns },
                    { "Patterns Includes", ImHexPath::PatternsInclude },
                    { "Plugins", ImHexPath::Plugins },
                    { "Python Scripts", ImHexPath::Python },
                    { "Yara Patterns", ImHexPath::Yara }
                }};

                ImGui::TableHeadersRow();
                for (const auto &[name, type] : PathTypes) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(name);

                    ImGui::TableNextColumn();
                    for (auto &path : hex::getPath(type))
                        ImGui::TextUnformatted(path.c_str());
                }

                ImGui::EndTable();
            }

            ImGui::EndPopup();
        }
    }

    void ViewHelp::drawPatternHelpPopup() {
        if (!this->m_patternHelpWindowOpen) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(450, 300), ImVec2(2000, 1000));
        if (ImGui::Begin(View::toWindowName("hex.view.help.pattern_cheat_sheet").c_str(), &this->m_patternHelpWindowOpen)) {
            ImGui::Text("ImHex Pattern Language Cheat Sheet");
            ImGui::Separator();
            ImGui::NewLine();

            drawTitle("Preprocessor directives");
            ImGui::TextWrapped(
                    "Preprocessor directives can be used to alter the code before it's being parsed. Supported are "
                    "#define to replace one string with another and #include to include a separate file");
            drawCodeSegment("preprocessor",
                            "#define HEADER_OFFSET 0x100\n"
                            "#include <cstdint.hexpat>\n"
                            "#include \"mypattern.hexpat\""
            );

            drawTitle("Pragma directives");
            ImGui::TextWrapped(
                    "Pragma directives are used to give ImHex additional information about the code being read. Currently "
                    "the following directives are supported.");
            drawCodeSegment("pragma",
                            "// Allow this file to be loaded as pattern when a file\n"
                            "// identified as application/x-executable gets loaded\n"
                            "#pragma MIME application/x-executable\n"
            );

            drawTitle("Built-in types");
            ImGui::TextWrapped(
                    "The following built-in types are available for use");
            drawCodeSegment("built-in",
                    "u8, s8, char, bool\n"
                    "u16, s16\n"
                    "u32, s32\n"
                    "u64, s64\n"
                    "u128, s128\n"
                    "float, double"
                    );

            drawTitle("Variables and Arrays");
            ImGui::TextWrapped(
                    "Normal variables as well as arrays are used to highlight and display values. "
                    "It is possible to create arrays within structs and unions that use the value of a previously "
                    "declared variable as size. In order to override the native / globally set endianess setting, every "
                    "type in a variable declaration may be prefixed with be or le to turn only it into a big or little endian type.");
            drawCodeSegment("vars arrays",
                    "u32 variable;\n"
                    "s8 string[16];\n"
                    "u8 customSizedArray[variable];\n"
                    "be u32 bigEndianVariable;"
            );

            drawTitle("Structs");
            ImGui::TextWrapped(
                    "To bundle multiple variables together, a struct can be used. To insert padding bytes which won't show "
                    "up in the pattern data view or be highlighted, use the padding[size] syntax.");
            drawCodeSegment("struct",
                "struct Header {\n"
                "   u32 magic;\n"
                "   u8 version;\n"
                "   padding[4];\n"
                "   Flags flags;\n"
                "};"
            );

            drawTitle("Variable placement");
            ImGui::TextWrapped(
                    "In order to highlight bytes and displaying their value in the pattern data window, "
                    "a variable needs to be created and placed in memory. The following line of code creates"
                    "a unsigned 32 bit variable named data and places it at offset 0x100."
                    );
            drawCodeSegment("var placement", "u32 data @ 0x100;");

            drawTitle("Unions");
            ImGui::TextWrapped(
                    "A union is used to make two or more variables occupy the same region of memory. "
                    "The union will have the size of the biggest contained variable.");
            drawCodeSegment("union",
                            "union Color {\n"
                            "   u32 rgba;\n"
                            "   Components components;\n"
                            "};"
            );

            drawTitle("Pointers");
            ImGui::TextWrapped(
                    "\"Another possible type of member in structs and unions are pointers. They are variables"
                    "whose value is used as an offset from the start of the file to locate the actual offset. "
                    "The leading type is treated as the data being pointed to and the trailing type as the size of the pointer.");
            drawCodeSegment("pointer",
                            "Data *data : u16;"
            );

            drawTitle("Built-in Functions");
            drawBuiltinFunction(
                "u64", "findSequence", ("u32 index, u8 ... bytes"),
                "find the `index`th occurence of the list of `bytes`"
            );
            drawBuiltinFunction(
                "u(size * 8)", "readUnsigned", ("u64 address, u8 size"),
                "read `size` bytes at `address` as little-endian unsigned integer"
            );
            drawBuiltinFunction(
                "s(size * 8)", "readSigned", ("u64 address, u8 size"),
                "read `size` bytes at `address` as little-endian signed integer"
            );
            drawBuiltinFunction(
                "u64", "alignTo", ("u64 value, u64 alignment"),
                "returns the smallest value divisible by `alignment` that is greater or equal to `value`"
            );
            drawBuiltinFunction(
                "void", "print", ("values..."),
                "output debugging information to pattern console"
            );
            drawBuiltinFunction(
                "void", "assert", ("condition, message"),
                "assert that a condition holds, aborting execution and displaying `message` if false"
            );
            drawBuiltinFunction(
                "void", "warnAssert", ("condition, message"),
                "assert that a condition holds, printing `message` as a warning if false"
            );
            drawBuiltinFunction(
                "u64", "dataSize", (""),
                "return the size of the current file"
            );

            drawTitle("Bitfields");
            ImGui::TextWrapped(
                    "To decode values that are stored in fields that don't follow the typical 8 bit alignment, bitfields can be used. "
                    "The size of these fields get specified in numbers of bits.");
            drawCodeSegment("bitfield",
                            "bitfield Permission {\n"
                            "   r : 1;\n"
                            "   w : 1;\n"
                            "   x : 1;\n"
                            "};"
            );

            drawTitle("Enum");
            ImGui::TextWrapped(
                    "If a value can only be a few specific values with special meaning, an enum can be used. "
                    "The underlying type has to be specified using a unsigned, built-in type after the name. "
                    "Entries can be listed with or without a value. The values start counting at zero and increase by one "
                    "for every next entry");
            drawCodeSegment("enum",
                    "enum OperatingSystem : u8 {\n"
                    "   Windows = 0x10,\n"
                    "   MacOSX,\n"
                    "   Linux = 'L'\n"
                    "};"
            );

            drawTitle("Using declarations");
            ImGui::TextWrapped(
                    "A using declaration can be used to create type aliases for already existing types. This can be "
                    "a built-in type, a struct, enum or another alias type.");
            drawCodeSegment("using",
                            "using magic_t = u32;"
            );

            drawTitle("Math Expressions");
            ImGui::TextWrapped(
                "In any place where a numeric value is required, a mathematical expression can be"
                " inserted. This can be as easy as 1 + 1 but can get much more complex as well by"
                " accessing values within structs or enum constants. These expressions work the"
                " same as in basically every other language as well with the following operators"
                " being supported:"
            );
            drawUnaryBulletPoint("+", "Addition");
            drawUnaryBulletPoint("-", "Subtraction");
            drawUnaryBulletPoint("*", "Multiplication");
            drawUnaryBulletPoint("/", "Division");
            drawUnaryBulletPoint("%", "Modulus");
            drawUnaryBulletPoint(">>", "Bit Right Shift");
            drawUnaryBulletPoint("<<", "Bit Left Shift");
            drawUnaryBulletPoint("&", "Bitwise AND");
            drawUnaryBulletPoint("|", "Bitwise OR");
            drawUnaryBulletPoint("^", "Bitwise XOR");
            drawUnaryBulletPoint("==", "Equality comparison");
            drawUnaryBulletPoint("!=", "Inequality comparison");
            drawUnaryBulletPoint(">", "Greater-than comparison");
            drawUnaryBulletPoint(">=", "Greater-or-equals comparison");
            drawUnaryBulletPoint("<", "Less-than comparison");
            drawUnaryBulletPoint("<=", "Less-or-Equal comparison");
            drawUnaryBulletPoint("&&", "Boolean AND");
            drawUnaryBulletPoint("||", "Boolean OR");
            drawUnaryBulletPoint("^^", "Boolean XOR");
            ImGui::Bullet(); ImGui::Text("a ? b : c - Ternary comparison");
            ImGui::Bullet(); ImGui::Text("$ - Current offset");

            drawTitle("Comments");
            ImGui::TextWrapped(
                    "To create a comment the C // or /*  */ syntax can be used. //-style comments end at the next new line "
                    "and /*-style comments only end when at the next */.");
            drawCodeSegment("comment",
                            "// This is a single line comment\n\n"
                            "/* This is a\n"
                            "multiline comment */"
            );
        }
        ImGui::End();
    }

    void ViewHelp::drawMathEvaluatorHelp() {
        if (!this->m_mathHelpWindowOpen) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(450, 300), ImVec2(2000, 1000));
        if (ImGui::Begin(View::toWindowName("hex.view.help.calc_cheat_sheet").c_str(), &this->m_mathHelpWindowOpen)) {
            ImGui::Text("ImHex Math Evaluator Cheat Sheet");
            ImGui::Separator();
            ImGui::NewLine();

            ImGui::TextWrapped("ImGui has a simple math evaluator / calculator built-in. It works by parsing the "
                               "expressions passed to it and displaying the result in the History table.");
            ImGui::NewLine();

            drawTitle("Basic Operators");
            ImGui::TextWrapped(
                    "The following basic mathematical operators are supported");
            drawCodeSegment("basicOperators",
                            "+   : Addition\n"
                            "-   : Subtraction\n"
                            "*   : Multiplication\n"
                            "/   : Division\n"
                            "%   : Modulus\n"
                            "**  : Exponentiation\n"
                            "()  : Brackets\n"
                            "x = : Variable assignment");

            drawTitle("Bitwise Operators");
            ImGui::TextWrapped(
                    "The following bitwise operators are supported");
            drawCodeSegment("bitwiseOperators",
                            "&  : Bitwise AND\n"
                            "|  : Bitwise OR\n"
                            "^  : Bitwise XOR\n"
                            "~  : Bitwise NOT\n"
                            "<< : Logical shift left\n"
                            ">> : Logical shift right\n"
                            "## : Bitwise concatenation");

            drawTitle("Boolean Operators");
            ImGui::TextWrapped(
                    "The following bitwise operators are supported");
            drawCodeSegment("booleanOperators",
                            "&& : Boolean AND\n"
                            "|| : Boolean OR\n"
                            "^^ : Boolean XOR\n"
                            "!  : Boolean NOT\n"
                            "== : Equality comparison\n"
                            "!= : Inequality comparison\n"
                            ">  : Greater than comparison\n"
                            "<  : Less than comparison\n"
                            ">= : Greater than or equals comparison\n"
                            "<= : Less than or equals comparison");

            drawTitle("Built-in Functions");
            ImGui::TextWrapped(
                    "The following functions are built into the evaluator");
            drawCodeSegment("functions",
                            "sin(x)      : Sine function\n"
                            "cos(x)      : Cosine function\n"
                            "tan(x)      : Tangent function\n"
                            "ceil(x)     : Rounds value to next bigger integer\n"
                            "floor(x)    : Rounds value to next smaller integer\n"
                            "sign(x)     : Returns the value's sign\n"
                            "abs(x)      : Absolute value\n"
                            "ln(x)       : Natural Logarithm\n"
                            "lb(x)       : Logarithm with base 2\n"
                            "log(x, b)   : Logarithm with custom base. Defaults to base 10\n"
                            "clear()     : Clear History and variables\n"
                            "read(a)     : Read 1 byte from loaded data\n"
                            "write(a, x) : Write 1 byte to loaded data");

        }
        ImGui::End();
    }

    void ViewHelp::drawContent() {
        if (!this->m_aboutWindowOpen && !this->m_mathHelpWindowOpen && !this->m_patternHelpWindowOpen)
            this->getWindowOpenState() = false;

        this->drawAboutPopup();
        this->drawPatternHelpPopup();
        this->drawMathEvaluatorHelp();
    }

    void ViewHelp::drawMenu() {
        if (ImGui::BeginMenu("hex.menu.help"_lang)) {
            if (ImGui::MenuItem("hex.view.help.about.name"_lang, "")) {
                View::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.view.help.about.name").c_str()); });
                this->m_aboutWindowOpen = true;
                this->getWindowOpenState() = true;
            }
            if (ImGui::MenuItem("hex.view.help.documentation"_lang, "")) {
                hex::openWebpage("https://imhex.werwolv.net/docs");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("hex.view.help.pattern_cheat_sheet"_lang, "")) {
                this->m_patternHelpWindowOpen = true;
                this->getWindowOpenState() = true;
            }
            if (ImGui::MenuItem("hex.view.help.calc_cheat_sheet"_lang, "")) {
                this->m_mathHelpWindowOpen = true;
                this->getWindowOpenState() = true;
            }
            ImGui::Separator();
            ImGui::EndMenu();
        }
    }

}
