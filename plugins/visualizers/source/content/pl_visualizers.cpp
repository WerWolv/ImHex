#include <hex/api/content_registry.hpp>

#include <pl/patterns/pattern.hpp>


namespace hex::plugin::visualizers {

    void drawLinePlotVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawScatterPlotVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawImageVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawBitmapVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void draw3DVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawSoundVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawCoordinateVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawTimestampVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawTableVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawDigitalSignalVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);

    void registerPatternLanguageVisualizers() {
        using ParamCount = pl::api::FunctionParameterCount;

        ContentRegistry::PatternLanguage::addVisualizer("line_plot",        drawLinePlotVisualizer,             ParamCount::exactly(1));
        ContentRegistry::PatternLanguage::addVisualizer("scatter_plot",     drawScatterPlotVisualizer,          ParamCount::exactly(2));
        ContentRegistry::PatternLanguage::addVisualizer("image",            drawImageVisualizer,                ParamCount::exactly(1));
        ContentRegistry::PatternLanguage::addVisualizer("bitmap",           drawBitmapVisualizer,               ParamCount::between(3, 4));
        ContentRegistry::PatternLanguage::addVisualizer("3d",               draw3DVisualizer,                   ParamCount::between(2, 6));
        ContentRegistry::PatternLanguage::addVisualizer("sound",            drawSoundVisualizer,                ParamCount::exactly(3));
        ContentRegistry::PatternLanguage::addVisualizer("coordinates",      drawCoordinateVisualizer,           ParamCount::exactly(2));
        ContentRegistry::PatternLanguage::addVisualizer("timestamp",        drawTimestampVisualizer,            ParamCount::exactly(1));
        ContentRegistry::PatternLanguage::addVisualizer("table",            drawTableVisualizer,                ParamCount::exactly(3));
        ContentRegistry::PatternLanguage::addVisualizer("digital_signal",   drawDigitalSignalVisualizer,        ParamCount::exactly(1));
    }

}
