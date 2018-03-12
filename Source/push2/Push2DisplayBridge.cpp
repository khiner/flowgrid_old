#include "Push2DisplayBridge.h"
#include "Push2Display.h"
#include <cstdint>

#include "Macros.h"

using namespace ableton;

Push2DisplayBridge::Push2DisplayBridge()
        : push2Display(nullptr),
          image(Image::RGB, Push2Display::WIDTH, Push2Display::HEIGHT, !K(clearImage)),
          graphics(image) {};

void Push2DisplayBridge::init(ableton::Push2Display &push2Display) {
    this->push2Display = &push2Display;
}

juce::Graphics &Push2DisplayBridge::getGraphics() {
    return graphics;
}

void Push2DisplayBridge::flip() {
    // Create a bitmap data to access the pixel rgb values
    juce::Image::BitmapData bitmapData(image, juce::Image::BitmapData::readOnly);

    // Get access to the pixel data
    Push2Display::pixel_t *data = push2Display->getPixelData();

    // Convert the pixels, applying the xor masking needed for the display to work
    static const uint16_t xOrMasks[2] = {0xf3e7, 0xffe7};

    for (int y = 0; y < Push2Display::HEIGHT; y++) {
        for (int x = 0; x < Push2Display::WIDTH; x++) {
            juce::Colour c = bitmapData.getPixelColour(x, y);
            const auto pixel = pixelFromRGB(c.getRed(), c.getGreen(), c.getBlue());
            *data++ = pixel ^ xOrMasks[x % 2];
        }
    }

    // Send the constructed push2 bitmap to the display
    push2Display->flip();
}
