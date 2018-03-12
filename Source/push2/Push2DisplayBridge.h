#pragma once

#include "Push2Display.h"
#include "Push2UsbCommunicator.h"
#include "../JuceLibraryCode/JuceHeader.h"

namespace ableton {
    /*!
     *  Implements a bridge between juce::Graphics and push2 display format.
     */
    class Push2DisplayBridge {
    public:
        Push2DisplayBridge(): image(Image::RGB, Push2Display::WIDTH, Push2Display::HEIGHT, false),
        graphics(image) {}

        juce::Graphics &getGraphics() {
            return graphics;
        }

        inline void writeFrameToDisplay() {
            static const Push2Display::pixel_t xOrMasks[2] = {0xf3e7, 0xffe7};
            for (int y = 0; y < Push2Display::HEIGHT; y++) {
                for (int x = 0; x < Push2Display::WIDTH; x++) {
                    usbCommunicator.setPixelValue(x, y, pixelFromColour(image.getPixelAt(x, y)) ^ xOrMasks[x % 2]);
                }
            }
            usbCommunicator.onFrameFillCompleted();
        }

        /*!
         * \return pixel_t value in push display format from (r, g, b)
         * The display uses 16 bit per pixels in a 5:6:5 format
         *      MSB                           LSB
         *      b b b b|b g g g|g g g r|r r r r
         */
        inline static Push2Display::pixel_t pixelFromColour(juce::Colour&& colour) {
            auto pixel = static_cast<Push2Display::pixel_t>((colour.getBlue() & 0xF8) >> 3);
            pixel <<= 6;
            pixel += (colour.getGreen() & 0xFC) >> 2;
            pixel <<= 5;
            pixel += (colour.getRed() & 0xF8) >> 3;
            return pixel;
        }

    private:
        juce::Image image;
        juce::Graphics graphics;
        Push2UsbCommunicator usbCommunicator;
    };
}
