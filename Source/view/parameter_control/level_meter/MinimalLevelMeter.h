#pragma once

#include "JuceHeader.h"
#include "LevelMeterSource.h"
#include "LevelMeter.h"

class MinimalLevelMeter : public LevelMeter {
public:
    MinimalLevelMeter(LevelMeter::Orientation orientation) : LevelMeter(), orientation(orientation) {
        setColour(LevelMeter::meterForegroundColour, Colours::black);
        setColour(LevelMeter::meterBackgroundColour, Colours::lightgrey.darker(0.2f));

        setColour(LevelMeter::meterMaxNormalColour, Colours::lightgrey);
        setColour(LevelMeter::meterMaxWarnColour, Colours::orange);
        setColour(LevelMeter::meterMaxOverColour, Colours::darkred);

//        setColour(LevelMeter::gainControlColour, findColour(Slider::thumbColourId));
        setColour(LevelMeter::gainControlColour, Colours::white);

        gainControl.setColours(findColour(LevelMeter::gainControlColour), findColour(LevelMeter::gainControlColour), findColour(LevelMeter::gainControlColour));
        gainControl.setOutline(findColour(LevelMeter::gainControlColour), 0);
    }

    void resized() override {
        const auto &meterBounds = getMeterBounds();
        const auto &localBounds = getLocalBounds();
        int gainPosition = orientation == vertical ?
                           meterBounds.getRelativePoint(0.0f, 1.0f - gainValue).y :
                           meterBounds.getRelativePoint(gainValue, 0.0f).x;

        const auto &gainControlBounds = orientation == vertical ?
                                        localBounds.withHeight(GAIN_CONTROL_WIDTH).withY(gainPosition - GAIN_CONTROL_WIDTH / 2) :
                                        localBounds.withWidth(GAIN_CONTROL_WIDTH).withX(gainPosition - GAIN_CONTROL_WIDTH / 2);
        Path p;
        p.addRectangle(gainControlBounds);
        gainControl.setBounds(gainControlBounds);
        gainControl.setShape(p, false, false, false);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MinimalLevelMeter)

    static constexpr int GAIN_CONTROL_WIDTH = 4;

    LevelMeter::Orientation orientation;

    float getValueForRelativePosition(juce::Point<int> relativePosition) override {
        return orientation == vertical ?
               std::clamp(1.0f - float(relativePosition.y) / float(getHeight()), 0.0f, 1.0f) :
               std::clamp(float(relativePosition.x) / float(getWidth()), 0.0f, 1.0f);
    }

    Rectangle<float> getMeterBounds() {
        const auto &r = getLocalBounds().toFloat();
        return orientation == vertical ?
               r.reduced(getWidth() * 0.1f, GAIN_CONTROL_WIDTH) :
               r.reduced(GAIN_CONTROL_WIDTH, getHeight() * 0.1f);
    }

    void drawMeterBars(Graphics &g, const LevelMeterSource *source) override {
        const int numChannels = source ? source->getNumChannels() : 1;
        auto meterBounds = getMeterBounds();
        const float shortDimension = orientation == vertical ? meterBounds.getWidth() : meterBounds.getHeight();
        const float meterWidth = shortDimension / numChannels;
        for (unsigned int channel = 0; channel < numChannels; ++channel) {
            const auto &meterBarBounds = orientation == vertical ?
                                         meterBounds.removeFromLeft(meterWidth).reduced(shortDimension * 0.1f, 0.0f) :
                                         meterBounds.removeFromTop(meterWidth).reduced(0.0f, shortDimension * 0.1f);
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

                const auto &fillBounds = orientation == vertical ?
                                         meterBarBounds.withHeight(rmsDbScaled * meterBarBounds.getHeight()) :
                                         meterBarBounds.withWidth(rmsDbScaled * meterBarBounds.getWidth());
                g.setColour(findColour(LevelMeter::meterForegroundColour));
                g.fillRect(fillBounds);
            }
        }
    }
};
