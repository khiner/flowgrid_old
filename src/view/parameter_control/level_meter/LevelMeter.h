#pragma once

#include "view/parameter_control/ParameterControl.h"
#include "LevelMeterSource.h"

class LevelMeter : public ParameterControl, private Timer {
public:
    enum Orientation {
        horizontal,
        vertical,
    };

    enum ColourIds {
        meterOutlineColour,       /**< Colour for the outlines of meter bars etc. */
        meterMaxNormalColour,     /**< Text/line colour for the max number, if under warn threshold */
        meterMaxWarnColour,       /**< Text/line colour for the max number, if between warn threshold and clip threshold */
        meterMaxOverColour,       /**< Text/line colour for the max number, if above the clip threshold */
        meterGradientLowColour,   /**< Colour for the meter bar under the warn threshold */
        meterGradientMidColour,   /**< Colour for the meter bar in the warn area */
        meterGradientMaxColour,   /**< Colour for the meter bar at the clip threshold */
    };

    explicit LevelMeter(Orientation orientation) :
            ParameterControl(), orientation(orientation),
            thumb("gain", {}, {}, {}), source(nullptr), refreshRate(24) {
        addAndMakeVisible(thumb);
        thumb.addMouseListener(this, true);
        startTimerHz(refreshRate);
    }

    ~LevelMeter() override {
        thumb.removeMouseListener(this);
        stopTimer();
    }

    void paint(Graphics &g) override {
        drawMeterBars(g, source);
        if (source) source->decayIfNeeded();
    }

    void timerCallback() override { repaint(); }

    void setMeterSource(LevelMeterSource *source) { this->source = source; }

protected:
    LevelMeter::Orientation orientation;
    ShapeButton thumb;

    float getValueForPosition(juce::Point<int> localPosition) const override {
        return orientation == vertical ?
               std::clamp(1.0f - float(localPosition.y) / float(getHeight()), 0.0f, 1.0f) :
               std::clamp(float(localPosition.x) / float(getWidth()), 0.0f, 1.0f);
    }

private:
    WeakReference<LevelMeterSource> source;
    int refreshRate;

    virtual void drawMeterBars(Graphics &g, const LevelMeterSource *source) = 0;
};
