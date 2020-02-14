#pragma once

#include "JuceHeader.h"

class DraggableValueLabel : public Slider {
public:
    DraggableValueLabel() : Slider(Slider::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox) {}

    void paint(Graphics &g) override {
            auto textArea = getLocalBounds().withSizeKeepingCentre(getWidth(), jmin(getHeight(), 32));
//        g.setColour(findColour(Slider::rotarySliderOutlineColourId));
            g.setColour(Colours::darkgrey);
            g.fillRect(textArea);
            g.setColour(findColour(ToggleButton::textColourId));
            g.drawFittedText(getTextFromValue(getValue()), textArea, Justification::centred, 1);
    }
};
