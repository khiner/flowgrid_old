#pragma once

#include "JuceHeader.h"
#include "LevelMeterSource.h"
#include "LevelMeter.h"

class MinimalLevelMeter : public LevelMeter {
public:
    MinimalLevelMeter(LevelMeter::Orientation orientation) : LevelMeter(), orientation(orientation) {
        setColour(LevelMeter::meterForegroundColour, Colours::whitesmoke);
        setColour(LevelMeter::meterBackgroundColour, Colours::darkgrey);
        setColour(LevelMeter::gainControlColour, Colours::whitesmoke.withAlpha(0.75f));

        setColour(LevelMeter::meterMaxNormalColour, Colours::lightgrey);
        setColour(LevelMeter::meterMaxWarnColour, Colours::orange);
        setColour(LevelMeter::meterMaxOverColour, Colours::darkred);

        removeChildComponent(&gainValueControl);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MinimalLevelMeter)

    LevelMeter::Orientation orientation;

    void drawMeterBars(Graphics &g, const LevelMeterSource *source) override {
        int numChannels = source ? source->getNumChannels() : 1;
        auto bounds = getLocalBounds().toFloat();
        const float meterWidth = (orientation == LevelMeter::vertical ? bounds.getWidth() : bounds.getHeight()) / numChannels;
        for (unsigned int channel = 0; channel < numChannels; ++channel) {

            Rectangle<float> meterBarBounds;
            if (orientation == LevelMeter::vertical) {
                meterBarBounds = bounds.removeFromLeft(meterWidth).reduced(getLocalBounds().getWidth() * 0.1f);
            } else {
                meterBarBounds = bounds.removeFromTop(meterWidth).reduced(getLocalBounds().getHeight() * 0.1f);
            }



            g.setColour(findColour(LevelMeter::meterBackgroundColour));
            g.fillRect(meterBarBounds);
            if (source != nullptr) {
                const static float infinity = -80.0f;
                float rmsLevel = source->getRMSLevel(channel);
//                std::cout << "rmsLevel: " << rmsLevel << std::endl;
                float rmsDb = Decibels::gainToDecibels(rmsLevel, infinity);
//                std::cout << "rmsDb: " << rmsDb << std::endl;
                float rmsDbScaled = (rmsDb - infinity) / -infinity;
//                std::cout << "rmsDbScaled: " << rmsDbScaled << std::endl;
                float peakDb = Decibels::gainToDecibels(source->getMaxLevel(channel), infinity);

                g.setColour(findColour(LevelMeter::meterForegroundColour));
                
                const auto &fillBounds = orientation == LevelMeter::vertical ?
                                         meterBarBounds.withHeight(rmsDbScaled * meterBarBounds.getHeight()) :
                                         meterBarBounds.withWidth(rmsDbScaled * meterBarBounds.getWidth());
                g.fillRect(fillBounds);

//                if (peakDb > -49.0) {
//                    g.setColour(findColour((peakDb > -0.3f) ? LevelMeter::meterMaxOverColour :
//                                           ((peakDb > -5.0) ? LevelMeter::meterMaxWarnColour :
//                                            LevelMeter::meterMaxNormalColour)));
//                    g.fillEllipse(bounds.getCentreX(), bounds.getCentreY(), bounds.getWidth() / 2, bounds.getHeight() / 2);
//                }
            }
        }
    }
};
