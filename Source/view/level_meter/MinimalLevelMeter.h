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

        setColour(LevelMeter::gainControlColour, Colours::whitesmoke);

        gainControl.setColours(findColour(LevelMeter::gainControlColour), findColour(LevelMeter::gainControlColour), findColour(LevelMeter::gainControlColour));
        gainControl.setOutline(findColour(LevelMeter::gainControlColour), 0);
    }

    void resized() override {
        int length = orientation == vertical ? getHeight() : getWidth();
        int gainPosition = int(length * (orientation == vertical ? 1.0f - gainValue : gainValue));
        const auto &gainControlBounds = orientation == vertical ?
                                        getLocalBounds().withHeight(4).withY(gainPosition - 2) :
                                        getLocalBounds().withWidth(4).withX(gainPosition - 2);
        Path p;
        p.addRectangle(gainControlBounds);
        gainControl.setBounds(gainControlBounds);
        gainControl.setShape(p, false, false, false);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MinimalLevelMeter)

    LevelMeter::Orientation orientation;

    float getValueForRelativePosition(juce::Point<int> relativePosition) override {
        return orientation == vertical ?
               std::clamp(1.0f - float(relativePosition.y) / float(getHeight()), 0.0f, 1.0f) :
               std::clamp(float(relativePosition.x) / float(getWidth()), 0.0f, 1.0f);
    }

    void drawMeterBars(Graphics &g, const LevelMeterSource *source) override {
        int numChannels = source ? source->getNumChannels() : 1;
        auto remainingBounds = getLocalBounds().toFloat();
        const float meterWidth = float(orientation == LevelMeter::vertical ? getWidth() : getHeight()) / numChannels;
        for (unsigned int channel = 0; channel < numChannels; ++channel) {

            const auto &meterBarBounds = orientation == LevelMeter::vertical ?
                                         remainingBounds.removeFromLeft(meterWidth).reduced(getWidth() * 0.1f) :
                                         remainingBounds.removeFromTop(meterWidth).reduced(getHeight() * 0.1f);
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

                const auto &fillBounds = orientation == LevelMeter::vertical ?
                                         meterBarBounds.withHeight(rmsDbScaled * meterBarBounds.getHeight()) :
                                         meterBarBounds.withWidth(rmsDbScaled * meterBarBounds.getWidth());
                g.setColour(findColour(LevelMeter::meterForegroundColour));
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
