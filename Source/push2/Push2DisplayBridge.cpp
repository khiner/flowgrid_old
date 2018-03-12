#include "Push2DisplayBridge.h"

using namespace ableton;

juce::Graphics &Push2DisplayBridge::getGraphics() {
    return graphics;
}

void Push2DisplayBridge::flip() {
    // Create a bitmap data to access the pixel rgb values
    juce::Image::BitmapData bitmapData(image, juce::Image::BitmapData::readOnly);
    Push2Display::pixel_t *data = dataSource;

    // Convert the pixels, applying the xor masking needed for the display to work
    static const uint16_t xOrMasks[2] = {0xf3e7, 0xffe7};

    for (int y = 0; y < Push2Display::HEIGHT; y++) {
        for (int x = 0; x < Push2Display::WIDTH; x++) {
            juce::Colour c = bitmapData.getPixelColour(x, y);
            const auto pixel = pixelFromRGB(c.getRed(), c.getGreen(), c.getBlue());
            *data++ = pixel ^ xOrMasks[x % 2];
        }
        // account for difference in USB line-width stride and display width stride
        data += UsbCommunicator::LINE_WIDTH - Push2Display::WIDTH;
    }
}
