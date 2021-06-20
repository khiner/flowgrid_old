#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

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

    ~ParameterControl() override = default;

    float getValue() const { return normalisableRange.convertFrom0to1(value); }

    void setValue(float newUnnormalisedValue, NotificationType notification) {
        float newValue = normalisableRange.convertTo0to1(newUnnormalisedValue);
        if (value != newValue) {
            value = newValue;
            if (notification == sendNotificationSync) {
                listeners.call(&Listener::parameterControlValueChanged, this);
            }
            resized(); // Could make this more efficient by only changing thumb position
            repaint();
        }
    }

    void setNormalisableRange(NormalisableRange<float> newRange) {
        normalisableRange = std::move(newRange);
    }

    void mouseDown(const MouseEvent &event) override {
        listeners.call(&Listener::parameterControlDragStarted, this);
    }

    void mouseDrag(const MouseEvent &event) override {
        float newValue = getValueForPosition(event.getEventRelativeTo(this).getPosition());
        setValue(normalisableRange.convertFrom0to1(newValue), sendNotificationSync);
    }

    void mouseUp(const MouseEvent &event) override {
        listeners.call(&Listener::parameterControlDragEnded, this);
    }

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void parameterControlValueChanged(ParameterControl *) = 0;
        virtual void parameterControlDragStarted(ParameterControl *) = 0;
        virtual void parameterControlDragEnded(ParameterControl *) = 0;
    };

    void addListener(Listener *listener) { listeners.add(listener); }
    void removeListener(Listener *listener) { listeners.remove(listener); }

protected:
    float value{0.5f};

    virtual float getValueForPosition(juce::Point<int> localPosition) const = 0;

private:
    ListenerList<Listener> listeners;
    NormalisableRange<float> normalisableRange;
};
