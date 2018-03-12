#pragma once

#include "Push2UsbCommunicator.h"
#include <memory>
#include <atomic>
#include "Macros.h"

namespace ableton {
    class Push2Display {
    public:
        using pixel_t = uint16_t;

        static const int WIDTH = 960;
        static const int HEIGHT = 160;
        static const int DATA_SOURCE_WIDTH = 1024;
        static const int DATA_SOURCE_HEIGHT = HEIGHT;

        Push2Display(): communicator(dataSource) {}

        pixel_t *getPixelData() {
            return dataSource;
        }

    private:
        pixel_t dataSource[DATA_SOURCE_WIDTH * DATA_SOURCE_HEIGHT]{};
        UsbCommunicator communicator;
    };
}