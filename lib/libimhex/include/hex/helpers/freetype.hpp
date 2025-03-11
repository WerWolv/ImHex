#pragma once
#include "imgui.h"
#include <vector>

namespace hex::ft {

    class Bitmap {
        ImU32 m_width;
        ImU32 m_height;
        ImU32 m_pitch;
        std::vector <ImU8> m_data;
    public:
        Bitmap(ImU32 width, ImU32 height, ImU32 pitch, ImU8 *data) : m_width(width), m_height(height), m_pitch(pitch), m_data(std::vector<ImU8>(data, data + pitch * height)) {}

        Bitmap(ImU32 width, ImU32 height, ImU32 pitch) : m_width(width), m_height(height), m_pitch(pitch) { m_data.resize(pitch * height); }

        void clear() { m_data.clear(); }

        ImU32 getWidth() const { return m_width; }

        ImU32 getHeight() const { return m_height; }

        ImU32 getPitch() const { return m_pitch; }

        const std::vector <ImU8> &getData() const { return m_data; }

        ImU8 *getData() { return m_data.data(); }

        void lcdFilter();
    };

    class RGBA {
    public:

        static ImU32 addAlpha(ImU8 r, ImU8 g, ImU8 b) {
            color.rgbaVec[0] = r;
            color.rgbaVec[1] = g;
            color.rgbaVec[2] = b;
            color.rgbaVec[3] = (r + g + b) / 3;//luminance
            return color.rgba;
        }

        RGBA(ImU8 r, ImU8 g, ImU8 b, ImU8 a) {
            color.rgbaVec[0] = r;
            color.rgbaVec[1] = b;
            color.rgbaVec[2] = g;
            color.rgbaVec[3] = a;
        }

        explicit RGBA(ImU32 rgba) {
            color.rgba = rgba;
        }

        union RGBAU {
            ImU8 rgbaVec[4];
            ImU32 rgba;
        };
        inline static RGBAU color;
    };
}