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

        Push2Display() {
            pixel_t *pData = dataSource;
            for (uint8_t line = 0; line < DATA_SOURCE_HEIGHT; line++) {
                memset(pData, 0, DATA_SOURCE_WIDTH * sizeof(pixel_t));
                pData += DATA_SOURCE_WIDTH;
            }
        }

        void init() {
            return communicator.init(dataSource);
        }

        // Transfers the bitmap into the output buffer sent to
        // the push display. The push display buffer has a larger stride
        // than the given pixel data.
        void flip() {
            const pixel_t *src = pixelData;
            pixel_t *dst = dataSource;

            for (uint8_t line = 0; line < DATA_SOURCE_HEIGHT; line++) {
                memcpy(dst, src, WIDTH * sizeof(pixel_t));
                src += WIDTH;
                dst += DATA_SOURCE_WIDTH;
            }
        }

        pixel_t *getPixelData() {
            return pixelData;
        }

    private:
        static const int DATA_SOURCE_WIDTH = 1024;
        static const int DATA_SOURCE_HEIGHT = HEIGHT;

        pixel_t dataSource[DATA_SOURCE_WIDTH * DATA_SOURCE_HEIGHT]{};
        UsbCommunicator communicator;

        pixel_t pixelData[WIDTH * HEIGHT]{}; /*!< static memory block to store the pixel values */
    };
}