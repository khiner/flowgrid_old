#include "MinimalLevelMeter.h"

MinimalLevelMeter::MinimalLevelMeter(LevelMeter::Orientation orientation) : LevelMeter(orientation) {
    thumb.setColours(findColour(thumbColourId), findColour(thumbColourId), findColour(thumbColourId));
    thumb.setOutline(findColour(thumbColourId), 0);
}

void MinimalLevelMeter::resized() {
    const auto &meterBounds = getMeterBounds();
    const auto &localBounds = getLocalBounds();
    int thumbPosition = orientation == vertical ?
                        meterBounds.getRelativePoint(0.0f, 1.0f - value).y :
                        meterBounds.getRelativePoint(value, 0.0f).x;

    const auto &thumbBounds = orientation == vertical ?
                              localBounds.withHeight(THUMB_WIDTH).withY(thumbPosition - THUMB_WIDTH / 2) :
                              localBounds.withWidth(THUMB_WIDTH).withX(thumbPosition - THUMB_WIDTH / 2);
    Path p;
    p.addRectangle(thumbBounds);
    thumb.setBounds(thumbBounds);
    thumb.setShape(p, false, false, false);
}

Rectangle<float> MinimalLevelMeter::getMeterBounds() {
    const auto &r = getLocalBounds().toFloat();
    return orientation == vertical ?
           r.reduced(getWidth() * 0.1f, THUMB_WIDTH) :
           r.reduced(THUMB_WIDTH, getHeight() * 0.1f);
}

void MinimalLevelMeter::drawMeterBars(Graphics &g, const LevelMeterSource *source) {
    const int numChannels = source ? source->getNumChannels() : 1;
    auto meterBounds = getMeterBounds();
    const float shortDimension = orientation == vertical ? meterBounds.getWidth() : meterBounds.getHeight();
    const float meterWidth = shortDimension / numChannels;
    for (unsigned int channel = 0; channel < numChannels; ++channel) {
        const auto &meterBarBounds = orientation == vertical ?
                                     meterBounds.removeFromLeft(meterWidth).reduced(shortDimension * 0.1f, 0.0f) :
                                     meterBounds.removeFromTop(meterWidth).reduced(0.0f, shortDimension * 0.1f);
        g.setColour(findColour(backgroundColourId));
        g.fillRect(meterBarBounds);
        if (source != nullptr) {
            const static float infinity = -80.0f;
            float rmsLevel = source->getRMSLevel(channel);
//                std::cout << "rmsLevel: " << rmsLevel << std::endl;
            float rmsDb = Decibels::gainToDecibels(rmsLevel, infinity);
//                std::cout << "rmsDb: " << rmsDb << std::endl;
            float rmsDbScaled = (rmsDb - infinity) / -infinity;
//                std::cout << "rmsDbScaled: " << rmsDbScaled << std::endl;

            const auto &fillBounds = orientation == vertical ?
                                     meterBarBounds.withHeight(rmsDbScaled * meterBarBounds.getHeight()) :
                                     meterBarBounds.withWidth(rmsDbScaled * meterBarBounds.getWidth());
            g.setColour(findColour(foregroundColourId));
            g.fillRect(fillBounds);
        }
    }
}
