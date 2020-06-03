#pragma once

#include "JuceHeader.h"
#include "LevelMeterSource.h"

class LevelMeter : public Component, private Timer {
public:
    enum Orientation {
        horizontal,
        vertical,
    };

    enum ColourIds {
        backgroundColour,         /**< Background colour */
        meterForegroundColour,    /**< Unused, will eventually be removed */
        meterOutlineColour,       /**< Colour for the outlines of meter bars etc. */
        meterBackgroundColour,    /**< Background colour for the actual meter bar and the max number */
        meterMaxNormalColour,     /**< Text/line colour for the max number, if under warn threshold */
        meterMaxWarnColour,       /**< Text/line colour for the max number, if between warn threshold and clip threshold */
        meterMaxOverColour,       /**< Text/line colour for the max number, if above the clip threshold */
        meterGradientLowColour,   /**< Colour for the meter bar under the warn threshold */
        meterGradientMidColour,   /**< Colour for the meter bar in the warn area */
        meterGradientMaxColour,   /**< Colour for the meter bar at the clip threshold */
        gainControlColour,
    };

    LevelMeter() : gainControl("gain", {}, {}, {}), source(nullptr), refreshRate(24) {
        gainControl.addMouseListener(this, false);

        addAndMakeVisible(gainControl);

        startTimerHz(refreshRate);
    }

    ~LevelMeter() override {
        gainControl.removeMouseListener(this);
        stopTimer();
    }

    float getValue() const { return normalisableRange.convertFrom0to1(gainValue); }

    void setValue(float newUnnormalisedValue, NotificationType notification) {
        float newValue = normalisableRange.convertTo0to1(newUnnormalisedValue);
        if (gainValue != newValue) {
            gainValue = newValue;
            if (notification == sendNotificationSync)
                listeners.call([this](Listener &l) { l.levelMeterValueChanged(this); });
            resized();
        }
    }

    void setNormalisableRange(NormalisableRange<float> newRange) {
        normalisableRange = std::move(newRange);
    }

    void paint(Graphics &g) override {
        Graphics::ScopedSaveState saved(g);

        drawMeterBars(g, source);

        if (source)
            source->decayIfNeeded();
    }

    void timerCallback() override { repaint(); }

    void setMeterSource(LevelMeterSource *source) {
        this->source = source;
    }

    void mouseDrag(const MouseEvent &event) override {
        if (event.originalComponent == &gainControl) {
            float newValue = getValueForRelativePosition(event.getEventRelativeTo(this).getPosition());
            setValue(normalisableRange.convertFrom0to1(newValue), sendNotificationSync);
        }
    }

    class Listener {
    public:
        virtual ~Listener() {}

        virtual void levelMeterValueChanged(LevelMeter *) = 0;
    };

    void addListener(Listener *listener) { listeners.add(listener); }

    void removeListener(Listener *listener) { listeners.remove(listener); }

protected:
    ShapeButton gainControl;
    float gainValue{0.5f};

    virtual float getValueForRelativePosition(juce::Point<int> relativePosition) {
        return std::clamp(1.0f - float(relativePosition.y) / float(getHeight()), 0.0f, 1.0f);
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)

    WeakReference<LevelMeterSource> source;
    ListenerList<Listener> listeners;

    int refreshRate;

    NormalisableRange<float> normalisableRange;

    virtual void drawMeterBars(Graphics &g, const LevelMeterSource *source) = 0;
};
