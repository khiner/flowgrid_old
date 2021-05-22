#pragma once

#include "Push2Display.h"
#include "Push2UsbCommunicator.h"

/*!
 *  Implements a bridge between juce::Graphics and push2 display format.
 */
class Push2DisplayBridge {
public:
    static const int PUSH_2_VENDOR_ID = 0x2982;
    static const int PUSH_2_PRODUCT_ID = 0x1967;

    Push2DisplayBridge() : image(Image::RGB, Push2Display::WIDTH, Push2Display::HEIGHT, false),
                           graphics(image), usbCommunicator(PUSH_2_VENDOR_ID, PUSH_2_PRODUCT_ID) {}

    juce::Graphics &getGraphics() { return graphics; }

    inline void writeFrameToDisplay() {
        if (!usbCommunicator.isValid()) return;

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
    inline static Push2Display::pixel_t pixelFromColour(juce::Colour &&colour) {
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
