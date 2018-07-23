#pragma once

#include <Utilities.h>
#include "JuceHeader.h"

class GraphEditorTrack;

class GraphEditorProcessor : public Component, public Utilities::ValueTreePropertyChangeListener {
public:
    GraphEditorProcessor(ValueTree v, GraphEditorTrack &at)
            : state(std::move(v)), track(at) {
        state.addListener(this);
    }

    void paint(Graphics &g) override {
    }

    ValueTree state;

private:
    GraphEditorTrack &track;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v == state)
            if (i == IDs::name || i == IDs::colour)
                repaint();
    }
};
