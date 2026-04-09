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

    void resize(int width, int height);
    void load(std::string_view s, int x, int y, bool erase=true);

    int width() const {
        return m_width;
    }

    int height() const {
        return m_height;
    }

    bool get(int x, int y) {
        if ((x >= 0 && x < m_width) && (y >= 0 && y < m_height)) {
            return m_petriDish[m_currentBuffer + x + y*m_width] & 0x80;
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

    using CellType = unsigned char;
    using PetriDishType = std::vector<CellType>;
    PetriDishType m_petriDish;
};

}
