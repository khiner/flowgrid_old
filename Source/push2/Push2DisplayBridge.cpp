#include "Push2DisplayBridge.h"

using namespace ableton;

juce::Graphics &Push2DisplayBridge::getGraphics() {
    return graphics;
}

void Push2DisplayBridge::flip() {
    // Create a bitmap data to access the pixel rgb values
    juce::Image::BitmapData bitmapData(image, juce::Image::BitmapData::readOnly);

    static const Push2Display::pixel_t xOrMasks[2] = {0xf3e7, 0xffe7};
    for (int y = 0; y < Push2Display::HEIGHT; y++) {
        for (int x = 0; x < Push2Display::WIDTH; x++) {
            communicator.setPixelValue(x, y, pixelFromColour(bitmapData.getPixelColour(x, y)) ^ xOrMasks[x % 2]);
        }
    }
    communicator.onFrameFillCompleted();
}
