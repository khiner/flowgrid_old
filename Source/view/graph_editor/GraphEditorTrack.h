#pragma once

#include <Utilities.h>
#include <Identifiers.h>
#include <view/ColourChangeButton.h>
#include "JuceHeader.h"
#include "GraphEditorProcessors.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener {
public:
    explicit GraphEditorTrack(ValueTree v) : state(std::move(v)) {
        addAndMakeVisible(*(processors = std::make_unique<GraphEditorProcessors>(*this, state)));
        state.addListener(this);
    }

    Colour getColour() const {
        return Colour::fromString(state[IDs::colour].toString());
    }

    void resized() override {
        auto r = getLocalBounds();
        processors->setBounds(r);
    }

    void paint(Graphics &g) override {
//        g.setColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::defaultText, Colours::black));
//        g.setFont(jlimit(8.0f, 15.0f, getHeight() * 0.9f));
//        g.drawText(state[IDs::name].toString(), r.removeFromLeft(clipX).reduced(4, 0), Justification::centredLeft, true);
    }

    ValueTree state;

private:
    std::unique_ptr<GraphEditorProcessors> processors;

    void valueTreePropertyChanged(juce::ValueTree &v, const juce::Identifier &i) override {
        if (v == state)
            if (i == IDs::name || i == IDs::colour)
                repaint();
    }
};
