#pragma once

#include "JuceHeader.h"

class Push2ProcessorSelector : public Component {
public:
    explicit Push2ProcessorSelector(StringArray &processorIds) : processorIds(processorIds) {
        for (const String &processorId : processorIds) {
            auto *label = new Label(processorId, processorId);
            addAndMakeVisible(label);
            label->setJustificationType(Justification::centred);
            processorIdLabels.add(label);
        }
    }

    const String& selectProcessor(int index) {
        return processorIds[index];
    }

    void resized() override {
        auto r = getBounds().withHeight(getHeight() / 8);
        for (auto *processorIdLabel : processorIdLabels) {
            processorIdLabel->setBounds(r.removeFromLeft(getWidth() / 8));
        }
    }

private:
    StringArray &processorIds;
    OwnedArray<Label> processorIdLabels;
};
