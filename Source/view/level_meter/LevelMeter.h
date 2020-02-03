#pragma once

#include "JuceHeader.h"
#include "LevelMeterSource.h"

class LevelMeter : public Component, private Timer {
public:
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
        setColour(LevelMeter::meterForegroundColour, Colours::green);
        setColour(LevelMeter::meterOutlineColour, Colours::lightgrey);
        setColour(LevelMeter::meterBackgroundColour, Colours::darkgrey);
        setColour(LevelMeter::meterMaxNormalColour, Colours::lightgrey);
        setColour(LevelMeter::meterMaxWarnColour, Colours::orange);
        setColour(LevelMeter::meterMaxOverColour, Colours::darkred);
        setColour(LevelMeter::meterGradientLowColour, Colours::green);
        setColour(LevelMeter::meterGradientMidColour, Colours::yellow);
        setColour(LevelMeter::meterGradientMaxColour, Colours::red);
        setColour(LevelMeter::gainControlColour, Colours::lightgrey.withAlpha(0.75f));

        gainValueControl.setColour(ComboBox::outlineColourId, Colours::transparentBlack);
        gainValueControl.setColour(TextButton::buttonColourId, findColour(Slider::thumbColourId).withAlpha(0.8f));

        gainValueControl.addMouseListener(this, false);
        addAndMakeVisible(gainValueControl);

        startTimerHz(refreshRate);
    }

    ~LevelMeter() override {
        gainValueControl.removeMouseListener(this);
        stopTimer();
    }

    float getValue() const { return normalisableRange.convertFrom0to1(gainValue); }

    float getRawValue() const { return gainValue; }

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

    void setRawValue(float newNormalizedValue, NotificationType notificationType) {
        setValue(normalisableRange.convertFrom0to1(newNormalizedValue), notificationType);
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
        updateMeterGradients();
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

        virtual void levelMeterValueChanged(LevelMeter*) = 0;
    };

    void addListener(Listener *listener) { listeners.add(listener); }

    void removeListener(Listener *listener) { listeners.remove(listener); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)

    WeakReference<LevelMeterSource> source;
    ColourGradient verticalGradient;
    ListenerList<Listener> listeners;

    TextButton gainValueControl;
    
    int refreshRate;

    float gainValue {0.5f};
    NormalisableRange<float> normalisableRange;

    void updateMeterGradients() {
        verticalGradient.clearColours();
    }

    void drawMeterBars(Graphics &g, const LevelMeterSource *source) {
        int numChannels = source ? source->getNumChannels() : 1;
        auto bounds = getLocalBounds().toFloat();
        const float width = bounds.getWidth() / numChannels;
        for (unsigned int channel = 0; channel < numChannels; ++channel) {
            const auto meterBarBounds = bounds.removeFromLeft(width).reduced(3);
            g.setColour(findColour(meterBackgroundColour));
            g.fillRect(meterBarBounds);
//            g.setColour(findColour(meterOutlineColour));
//            g.drawRect(meterBarBounds, 1.0);
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
