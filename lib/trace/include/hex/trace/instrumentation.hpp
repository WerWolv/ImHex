#pragma once

#if defined(IMHEX_USE_INSTRUMENTATION)

    #include <tracy/Tracy.hpp>

    #define IMHEX_TRACE_SCOPE ZoneScoped
    #define IMHEX_TRACE_SCOPE_NAME(name) ZoneScoped; ZoneName(name, strlen(name))
    #define IMHEX_START_FRAME_MARK FrameMarkStart("Frame")
    #define IMHEX_END_FRAME_MARK FrameMarkEnd("Frame")
    #define IMHEX_TRACE_MESSAGE(message) TracyMessage(message, strlen(message))

#else

    #define IMHEX_TRACE_SCOPE_NAME(name)
    #define IMHEX_END_FRAME_MARK

#endif