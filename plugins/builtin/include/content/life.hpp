#pragma once
// life.hpp
//
// A simple implementation of Conway's Game of Life
//

#include <cstddef>
#include <vector>
#include <string_view>

namespace hex::plugin::builtin {

class Life {
public:
    Life(int width, int height);

    void parse(std::string_view s, int x, int y);

    bool get(int x, int y) {
        if ((x >= 0 && x < m_width) && (y >= 0 && y < m_height)) {
            return m_world[m_currentBuffer + x + y*m_width] & 0x80;
        }

        return false;
    }

    void birth(int x, int y);
    void death(int x, int y);
    void tick();

private:
    void _set(int x, int y, int diff, size_t buffer);

    int m_width;
    int m_height;
    size_t m_currentBuffer;
    size_t m_otherBuffer;
    size_t m_secondBuffer;
    std::vector<int> m_world;
};

}
