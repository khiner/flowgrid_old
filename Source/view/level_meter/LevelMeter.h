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

    LevelMeter() : source(nullptr), refreshRate(24) {
        gainValueControl.addMouseListener(this, false);
        gainValueControl.setColour(ComboBox::outlineColourId, Colours::transparentBlack);
        gainValueControl.setColour(TextButton::buttonColourId, findColour(Slider::thumbColourId).withAlpha(0.8f));

        addAndMakeVisible(gainValueControl);

        startTimerHz(refreshRate);
    }

    ~LevelMeter() override {
        gainValueControl.removeMouseListener(this);
        stopTimer();
    }

    float getValue() const { return normalisableRange.convertFrom0to1(gainValue); }

    void setValue(float newUnnormalizedValue, NotificationType notification) {
        jassert(notification == dontSendNotification || notification == sendNotificationSync);

        float newValue = normalisableRange.convertTo0to1(newUnnormalizedValue);
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

    void resized() override {
        int gainControlY = int(getHeight() * (1.0f - gainValue));
        gainValueControl.setBounds(getLocalBounds().withHeight(10).withY(gainControlY - 5));
    }

    void timerCallback() override { repaint(); }

    void setMeterSource(LevelMeterSource *source) {
        this->source = source;
    }

    void mouseDrag(const MouseEvent &event) override {
        if (event.originalComponent == &gainValueControl) {
            float newValue = std::clamp(1.0f - float(event.getEventRelativeTo(this).getPosition().y) / float(getHeight()), 0.0f, 1.0f);
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
    TextButton gainValueControl;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)

    WeakReference<LevelMeterSource> source;
    ListenerList<Listener> listeners;

    int refreshRate;

    float gainValue{0.5f};
    NormalisableRange<float> normalisableRange;

    virtual void drawMeterBars(Graphics &g, const LevelMeterSource *source) = 0;
};
