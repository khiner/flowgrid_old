#pragma once

#include "JuceHeader.h"
#include "LevelMeter.h"
#include "LevelMeterSource.h"

class SegmentedLevelMeter : public LevelMeter {
public:
    SegmentedLevelMeter() : LevelMeter() {
        setColour(LevelMeter::meterForegroundColour, Colours::green);
        setColour(LevelMeter::meterOutlineColour, Colours::lightgrey);
        setColour(LevelMeter::meterBackgroundColour, Colours::darkgrey);
        setColour(LevelMeter::meterMaxNormalColour, Colours::lightgrey);
        setColour(LevelMeter::meterMaxWarnColour, Colours::orange);
        setColour(LevelMeter::meterMaxOverColour, Colours::darkred);
        setColour(LevelMeter::meterGradientLowColour, Colours::green);
        setColour(LevelMeter::meterGradientMidColour, Colours::yellow);
        setColour(LevelMeter::meterGradientMaxColour, Colours::red);

        setColour(LevelMeter::gainControlColour, findColour(Slider::thumbColourId).withAlpha(0.8f));

//        gainControl.setFill(findColour(LevelMeter::gainControlColour));
//        gainControl.setStrokeThickness(0);
    }

    void resized() override {
//        int gainControlY = int(getHeight() * (1.0f - gainValue));
//        gainControl.setTransformToFit(getLocalBounds().toFloat().withHeight(10).withY(gainControlY - 5), RectanglePlacement::stretchToFit);
        verticalGradient.clearColours();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedLevelMeter)

    ColourGradient verticalGradient;

    void drawMeterBars(Graphics &g, const LevelMeterSource *source) override {
        int numChannels = source ? source->getNumChannels() : 1;
        auto bounds = getLocalBounds().toFloat();
        const float width = bounds.getWidth() / numChannels;
        for (unsigned int channel = 0; channel < numChannels; ++channel) {
            const auto meterBarBounds = bounds.removeFromLeft(width).reduced(3);
            g.setColour(findColour(meterBackgroundColour));
            g.fillRect(meterBarBounds);
            if (source != nullptr) {
                const static float infinity = -80.0f;
                float rmsDb = Decibels::gainToDecibels(source->getRMSLevel(channel), infinity);
                float peakDb = Decibels::gainToDecibels(source->getMaxLevel(channel), infinity);

                const Rectangle<float> floored(ceilf(meterBarBounds.getX()) + 1.0f, ceilf(meterBarBounds.getY()) + 1.0f,
                                               floorf(meterBarBounds.getRight()) -
                                               (ceilf(meterBarBounds.getX() + 2.0f)),
                                               floorf(meterBarBounds.getBottom()) -
                                               (ceilf(meterBarBounds.getY()) + 2.0f));

                if (verticalGradient.getNumColours() < 2) {
                    verticalGradient = ColourGradient(findColour(meterGradientLowColour),
                                                      floored.getX(), floored.getBottom(),
                                                      findColour(meterGradientMaxColour),
                                                      floored.getX(), floored.getY(), false);
                    verticalGradient.addColour(0.5, findColour(meterGradientLowColour));
                    verticalGradient.addColour(0.75, findColour(meterGradientMidColour));
                }
                g.setGradientFill(verticalGradient);
                g.fillRect(floored.withTop(floored.getY() + rmsDb * floored.getHeight() / infinity));

                if (peakDb > -49.0) {
                    g.setColour(findColour((peakDb > -0.3f) ? meterMaxOverColour :
                                           ((peakDb > -5.0) ? meterMaxWarnColour :
                                            meterMaxNormalColour)));
                    g.drawHorizontalLine(int(floored.getY() + jmax(peakDb * floored.getHeight() / infinity, 0.0f)),
                                         floored.getX(), floored.getRight());
                }
            }
        }
    }
};
