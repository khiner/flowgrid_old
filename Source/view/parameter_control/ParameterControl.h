#pragma once

#include "JuceHeader.h"

class ParameterControl : public Component {
public:
    enum ColourIds {
        backgroundColourId,
        foregroundColourId,
        thumbColourId,
    };

    ParameterControl() {
        setColour(foregroundColourId, Colours::black);
        setColour(backgroundColourId, Colours::lightgrey.darker(0.2f));
        setColour(thumbColourId, Colours::white);
    }

    ~ParameterControl() override {
    }

    float getValue() const { return normalisableRange.convertFrom0to1(value); }

    void setValue(float newUnnormalisedValue, NotificationType notification) {
        float newValue = normalisableRange.convertTo0to1(newUnnormalisedValue);
        if (value != newValue) {
            value = newValue;
            if (notification == sendNotificationSync)
                listeners.call([this](Listener &l) { l.parameterControlValueChanged(this); });
            resized(); // Could make this more efficient by only changing thumb position
            repaint();
        }
    }

    void setNormalisableRange(NormalisableRange<float> newRange) {
        normalisableRange = std::move(newRange);
    }

    void mouseDrag(const MouseEvent &event) override {
        float newValue = getValueForPosition(event.getEventRelativeTo(this).getPosition());
        setValue(normalisableRange.convertFrom0to1(newValue), sendNotificationSync);
    }

    class Listener {
    public:
        virtual ~Listener() {}

        virtual void parameterControlValueChanged(ParameterControl *) = 0;
    };

    void addListener(Listener *listener) { listeners.add(listener); }

    void removeListener(Listener *listener) { listeners.remove(listener); }

protected:
    float value{0.5f};

    virtual float getValueForPosition(juce::Point<int> localPosition) const = 0;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterControl)

    ListenerList<Listener> listeners;

    NormalisableRange<float> normalisableRange;
};
