#include "JuceToPush2DisplayBridge.h"

using namespace ableton;

//------------------------------------------------------------------------------

Push2DisplayBridge::Push2DisplayBridge()
        : push2Display_(nullptr),
          image_(Image::RGB, Push2DisplayBitmap::kWidth, Push2DisplayBitmap::kHeight, !K(clearImage)),
          graphics_(image_) {};


//------------------------------------------------------------------------------

NBase::Result Push2DisplayBridge::Init(ableton::Push2Display &display) {
    push2Display_ = &display;
    return NBase::Result::NoError;
}


//------------------------------------------------------------------------------

juce::Graphics &Push2DisplayBridge::GetGraphic() {
    return graphics_;
}


//------------------------------------------------------------------------------

void Push2DisplayBridge::Flip() {
    // Make sure the class was properly initialised
    assert(push2Display_);

    // Create a bitmap data to access the pixel rgb values
    juce::Image::BitmapData bmData(image_, juce::Image::BitmapData::readOnly);

    // Create a push display bitmap and get access to the pixel data
    Push2DisplayBitmap g;
    Push2DisplayBitmap::pixel_t *data = g.PixelData();

    const int sizex = std::min(g.GetWidth(), image_.getWidth());
    const int sizey = std::min(g.GetHeight(), image_.getHeight());


    // Convert the pixels, applying the xor masking needed for the display to work
    static const uint16_t xOrMasks[2] = {0xf3e7, 0xffe7};

    for (int y = 0; y < sizey; y++) {
        for (int x = 0; x < sizex; x++) {
            juce::Colour c = bmData.getPixelColour(x, y);
            const auto pixel = Push2DisplayBitmap::SPixelFromRGB(c.getRed(), c.getGreen(), c.getBlue());
            *data++ = pixel ^ xOrMasks[x % 2];
        }
        data += (g.GetWidth() - sizex);
    }

    // Send the constructed push2 bitmap to the display
    NBase::Result result = push2Display_->Flip(g);
    assert(result.Succeeded());
}
