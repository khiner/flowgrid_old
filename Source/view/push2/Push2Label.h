#pragma once

#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"

class Push2Label : public Label {
public:
    Push2Label(int position, bool top, Push2MidiCommunicator& push2MidiCommunicator) :
            Label(), position(position), top(top), push2MidiCommunicator(push2MidiCommunicator) {
        updateColours();
    }

    void setVisible(bool visible) override {
        Label::setVisible(visible);
        updatePush2Button();
    }

    // Only a single color is needed to specify text & background color for Push 2 Labels.
    // If the label is selected, the background colour will be this colour and the text will be black.
    // If the label is not selected, the background colour will be black and the text will be this colour.
    void setMainColour(const Colour &colour) {
        this->colour = colour;
        updateColours();
    }

    bool isSelected() {
        return selected;
    }

    void setSelected(bool selected) {
        this->selected = selected;
        updateColours();
    }

private:
    int position;
    bool top;
    bool selected { false };
    Colour colour { Colours::white };

    Push2MidiCommunicator& push2MidiCommunicator;

    void updateColours() {
        if (selected) {
            setColour(Label::backgroundColourId, colour);
            setColour(Label::textColourId, Colours::black);
        } else {
            setColour(Label::backgroundColourId, Colours::black);
            setColour(Label::textColourId, colour);
        }
        updatePush2Button();
    }

    void updatePush2Button() {
        const Colour& buttonColour = selected ? Colours::white : colour;
        if (top) {
            if (isVisible())
                push2MidiCommunicator.setAboveScreenButtonColour(position, buttonColour);
            else
                push2MidiCommunicator.setAboveScreenButtonEnabled(position, false);
        } else {
            if (isVisible())
                push2MidiCommunicator.setBelowScreenButtonColour(position, buttonColour);
            else
                push2MidiCommunicator.setBelowScreenButtonEnabled(position, false);
        }
    }
};
