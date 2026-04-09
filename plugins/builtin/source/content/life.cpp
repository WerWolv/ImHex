// life.cpp
//
// A simple implementation of Conway's Game of Life
//

#include <content/life.hpp>
#include <cstring>
#include <charconv>
#include <utility>
#include <algorithm>

namespace hex::plugin::builtin {

Life::Life(int width, int height) :
    m_width(width), m_height(height),
    m_currentBuffer(0),
    m_otherBuffer(width * height),
    m_world(width * height * 2)
{
}

void Life::resize(int width, int height) {
    std::vector<int> resized(width*height*2);

    const size_t other = width*height;
    for (int y=0; y<std::min(height, m_height); ++y) {
        for (int x=0; x<std::min(width, m_width); ++x) {
            resized[x + y*width] = m_world[m_currentBuffer + x + y*m_width];
        }
    }

    m_width = width;
    m_height = height;
    m_currentBuffer = 0;
    m_otherBuffer = other;
    m_world = std::move(resized);
}

void Life::load(std::string_view s, int x, int y, bool erase) {
    int xx = x;
    int yy = y;

    if (erase) {
        memset(&m_world[m_currentBuffer], 0, sizeof(int) * m_width * m_height);
    }

    for (size_t i = 0; i < s.size(); ++i) {
        int reps = 1;
        auto [ptr, ec] = std::from_chars(s.data() + i, s.data() + s.size(), reps);
        if (ec == std::errc{}) {
            i = ptr - s.data();
            if (i >= s.size()) {
                return; // rle count with nothing after it
            }
        }

        char ch = *ptr;
        switch (ch) {
        case 'b':
            xx += reps;
            break;

        case 'o':
            for (int r = 0; r < reps; ++r) {
                birth(xx++, yy);
            }
            break;

        case '$':
            xx = x;
            yy += reps;
            break;

        case '!':
            return;

        default:
            return; // illegal char
        }
    }
}

void Life::birth(int x, int y) {
    if ((x >= 0 && x < m_width) && (y >= 0 && y < m_height)) {
        _set(x, y, 1, m_currentBuffer);
    }
}

void Life::death(int x, int y) {
    if ((x >= 0 && x < m_width) && (y >= 0 && y < m_height)) {
        _set(x, y, -1, m_currentBuffer);
    }
}

void Life::tick() {
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            int cell = m_world[m_currentBuffer + x + y * m_width];
            int neighbours = cell & 0x7f;

            if (cell & 0x80) {
                // alive
                if (neighbours - 1 >= 2 && neighbours - 1 <= 3) {
                    _set(x, y, 1, m_otherBuffer); // survival
                }
            }
            else {
                // dead
                if (neighbours == 3) {
                    _set(x, y, 1, m_otherBuffer); // birth
                }
            }
        }
    }

    memset(&m_world[m_currentBuffer], 0, sizeof(int) * m_width * m_height);
    std::swap(m_currentBuffer, m_otherBuffer);
}

void Life::_set(int x, int y, int diff, size_t buffer) {
    const int xmin = std::max(0, x - 1);
    const int xmax = std::min(m_width - 1, x + 1);
    const int ymin = std::max(0, y - 1);
    const int ymax = std::min(m_height - 1, y + 1);

    for (int yy = ymin; yy <= ymax; ++yy) {
        for (int xx = xmin; xx <= xmax; ++xx) {
            m_world[buffer + xx + yy * m_width] += diff;
        }
    }

    int& cell = m_world[buffer + x + y * m_width];
    cell = (cell & 0x7f) | (diff == 1 ? 0x80 : 0x00);
}

}
