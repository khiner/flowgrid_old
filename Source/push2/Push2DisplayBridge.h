#pragma once

#include "Push2Display.h"
#include "Push2UsbCommunicator.h"
#include "Macros.h"
#include "../JuceLibraryCode/JuceHeader.h"

namespace ableton {
    /*!
     *  Implements a bridge between juce::Graphics and push2 display format.
     */
    class Push2DisplayBridge {
    public:
        Push2DisplayBridge(): image(Image::RGB, Push2Display::WIDTH, Push2Display::HEIGHT, !K(clearImage)),
        graphics(image), communicator(dataSource) {}

        /*!
         * Access a reference to the juce::Graphics of the bridge
         * the returned object can be used with juce's drawing primitives
         */
        juce::Graphics &getGraphics();

        /*!
         * Tells the bridge the drawing is done and the pixel data can be sent to
         * the push display
         */
        void flip();

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
        Push2Display::pixel_t dataSource[Push2UsbCommunicator::LINE_WIDTH * Push2UsbCommunicator::NUM_LINES]{};
        Push2UsbCommunicator communicator;
        juce::Image image;        /*< The image used to render the pixel data */
        juce::Graphics graphics;  /*< The graphics associated to the image */
        bool firstFrame = true;
    };
}
