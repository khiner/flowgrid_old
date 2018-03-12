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
            // account for difference in USB line-width stride and display width stride
            dataSource[y * UsbCommunicator::LINE_WIDTH + x] = pixelFromColour(bitmapData.getPixelColour(x, y)) ^ xOrMasks[x % 2];
        }
    }
    if (firstFrame) {
        communicator.startSending();
        firstFrame = false;
    }
}
