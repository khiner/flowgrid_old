#include "MinimalLevelMeter.h"

MinimalLevelMeter::MinimalLevelMeter(LevelMeter::Orientation orientation) : LevelMeter(orientation) {
    thumb.setColours(findColour(thumbColourId), findColour(thumbColourId), findColour(thumbColourId));
    thumb.setOutline(findColour(thumbColourId), 0);
}

void MinimalLevelMeter::resized() {
    const auto &meterBounds = getMeterBounds();
    const auto &localBounds = getLocalBounds();
    int thumbPosition = orientation == vertical ?
                        static_cast<int>(meterBounds.getRelativePoint(0.0f, 1.0f - value).y) :
                        static_cast<int>(meterBounds.getRelativePoint(value, 0.0f).x);
    const auto &thumbBounds = orientation == vertical ?
                              localBounds.withHeight(THUMB_WIDTH).withY(thumbPosition - THUMB_WIDTH / 2) :
                              localBounds.withWidth(THUMB_WIDTH).withX(thumbPosition - THUMB_WIDTH / 2);
    Path p;
    p.addRectangle(thumbBounds);
    thumb.setBounds(thumbBounds);
    thumb.setShape(p, false, false, false);
}

Rectangle<int> MinimalLevelMeter::getMeterBounds() {
    const auto &r = getLocalBounds();
    return orientation == vertical ?
           r.reduced(getWidth() / 10, THUMB_WIDTH) :
           r.reduced(THUMB_WIDTH, getHeight() / 10);
}

void MinimalLevelMeter::drawMeterBars(Graphics &g, const LevelMeterSource *source) {
    const int numChannels = source ? static_cast<const int>(source->getNumChannels()) : 1;
    auto meterBounds = getMeterBounds();
    const int shortDimension = orientation == vertical ? meterBounds.getWidth() : meterBounds.getHeight();
    const int meterWidth = shortDimension / numChannels;
    for (int channel = 0; channel < numChannels; ++channel) {
        const auto &meterBarBounds = orientation == vertical ?
                                     meterBounds.removeFromLeft(meterWidth).reduced(shortDimension / 10, 0.0f) :
                                     meterBounds.removeFromTop(meterWidth).reduced(0.0f, shortDimension / 10);
        g.setColour(findColour(backgroundColourId));
        g.fillRect(meterBarBounds);
        if (source != nullptr) {
            const static float infinity = -80.0f;
            float rmsLevel = source->getRMSLevel(static_cast<unsigned int>(channel));
//                std::cout << "rmsLevel: " << rmsLevel << std::endl;
            float rmsDb = Decibels::gainToDecibels(rmsLevel, infinity);
//                std::cout << "rmsDb: " << rmsDb << std::endl;
            float rmsDbScaled = (rmsDb - infinity) / -infinity;
//                std::cout << "rmsDbScaled: " << rmsDbScaled << std::endl;

            const auto &fillBounds = orientation == vertical ?
                                     meterBarBounds.withHeight(static_cast<int>(rmsDbScaled * static_cast<float>(meterBarBounds.getHeight()))) :
                                     meterBarBounds.withWidth(static_cast<int>(rmsDbScaled * static_cast<float>(meterBarBounds.getWidth())));
            g.setColour(findColour(foregroundColourId));
            g.fillRect(fillBounds);
        }
    }
}
