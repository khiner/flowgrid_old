#pragma once

#include "JuceHeader.h"

class Push2ProcessorSelector : public Component {
public:
    explicit Push2ProcessorSelector(const StringArray &allProcessorIds) {
        for (const String &processorId : allProcessorIds) {
            auto *label = new Label(processorId, processorId);
            addAndMakeVisible(label);
            label->setJustificationType(Justification::centred);
            processorIdLabels.add(label);
        }
    }

    void setProcessorIds(const StringArray &processorIds) {
        for (auto *label : processorIdLabels) {
            label->setVisible(processorIds.contains(label->getText()));
        }
    }

    const String selectProcessor(const ValueTree &track, int index) {
        return processorIdLabels[index]->getText();
    }

    void resized() override {
        auto r = getBounds().withHeight(getHeight() / 8);
        for (auto *processorIdLabel : processorIdLabels) {
            processorIdLabel->setBounds(r.removeFromLeft(getWidth() / 8));
        }
    }

private:
    OwnedArray<Label> processorIdLabels;
};
