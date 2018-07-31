#pragma once

#include "JuceHeader.h"
#include "view/UiColours.h"

class SolidToggleButton : public ToggleButton {
    void paintButton(Graphics& g, bool isMouseOverButton, bool isButtonDown) override {
        auto r = getLocalBounds();
        g.setColour(findColour(Slider::textBoxOutlineColourId));
        g.drawRect(r);
        r.reduce(1, 1);
        if (getToggleState()) {
            g.setColour(findColour(TextButton::buttonOnColourId));
        } else {
            g.setColour(findColour(TextButton::buttonColourId));
        }
        g.fillRect(r);
    }
};
