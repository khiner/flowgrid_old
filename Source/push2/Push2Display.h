#pragma once

#include <cstdint>

namespace ableton {
    namespace Push2Display {
        using pixel_t = uint16_t;

        static const int WIDTH = 960;
        static const int HEIGHT = 160;
        static const int DATA_SOURCE_WIDTH = 1024;
        static const int DATA_SOURCE_HEIGHT = HEIGHT;
    }
}
