#include "hex/helpers/freetype.hpp"
#include "imgui.h"

namespace hex::ft {

    void Bitmap::lcdFilter() {
        if (m_width * m_height == 0)
            return;
        std::vector <ImU8> h = {8, 0x4D, 0x55, 0x4D, 8};
        auto paddedWidth = m_width + 4;
        auto fWidth = paddedWidth + ((3 - (paddedWidth % 3)) % 3);
        auto fPitch = (fWidth + 3) & (-4);
        Bitmap f(fWidth, m_height, fPitch);
        //std::vector<std::vector<ImU8>> fir(3, std::vector<ImU8>(3, 0));
        // first output: h[0]*input[0]/255.0f;
        // 2nd output:  (h[1]*input[0]   + h[0]*input[1])/255.0f;
        // 3rd output:  (h[2]*input[0]   + h[1]*input[1]   + h[0]*input[2])/255.0f;
        // 4th output:  (h[1]*input[0]   + h[2]*input[1]   + h[1]*input[2]   + h[0]*input[3])/255.0f;
        // ith output:  (h[0]*input[5+i] + h[1]*input[i+6] + h[2]*input[i+7] + h[1]*input[i+8] + h[0]*input[i+9])/255.0f;
        // aap output:  (h[0]*input[N-4] + h[1]*input[N-3] + h[2]*input[N-2] + h[1]*input[N-1])/255.0f;
        // ap output:   (h[0]*input[N-3] + h[1]*input[N-2] + h[2]*input[N-1])/255.0f;
        // p output:    (h[0]*input[N-2] + h[1]*input[N-1] )/255.0f;
        // last output:  h[0]*input[N-1]/255.0f;
        for (ImU32 y = 0; y < m_height; y++) {
            auto fp = y * fPitch;
            auto yp = y * m_pitch;
            bool done = false;
            for (ImU32 x = 0; x < 4; x++) {
                ImU32 head = 0;
                ImU32 tail = 0;
                if (m_width >= x + 1) {
                    for (ImU32 i = 0; i < x + 1; i++) {
                        head += h[x - i] * m_data[yp + i];
                        tail += h[i] * m_data[yp + m_width - 1 - x + i];
                    }
                    f.m_data[fp + x] = head >> 8;
                    f.m_data[fp + paddedWidth - 1 - x] = tail >> 8;
                } else {
                    done = true;
                    break;
                }
            }
            if (done)
                continue;
            for (ImU32 x = 4; x < paddedWidth - 4; x++) {
                ImU32 head = 0;
                for (ImS32 i = 0; i < 5; i++) {
                    head += h[i] * m_data[yp + i + x - 4];
                }
                f.m_data[fp + x] = head >> 8;
            }
        }
        clear();
        m_width = f.m_width;
        m_height = f.m_height;
        m_pitch = f.m_pitch;
        m_data = std::move(f.m_data);
    }

}