#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include "JuceHeader.h"
#include "Push2ComponentBase.h"
#include "view/processor_editor/ParametersPanel.h"

class Push2ProcessorView : public Push2ComponentBase {
public:
    explicit Push2ProcessorView(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator),
              pageLeftButton("Page left", 0.5, Colours::orangered),
              pageRightButton("Page right", 0.0, Colours::orangered) {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(pageLeftButton);
        addAndMakeVisible(pageRightButton);

        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1)).get());

        pageLeftButton.onClick = [this]() { pageLeft(); };

        pageRightButton.onClick = [this]() { pageRight(); };
    }

    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(20);
        pageRightButton.setBounds(top.removeFromRight(getWidth() / 8).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        pageLeftButton.setBounds(top.removeFromRight(getWidth() / 8).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        titleLabel.setBounds(top);

        parametersPanel->setBounds(r);
    }

    void setProcessor(StatefulAudioProcessorWrapper *const processorWrapper) {
        if (processorWrapper != nullptr) {
            titleLabel.setText(processorWrapper->processor->getName(), dontSendNotification);
        }
        parametersPanel->setProcessor(processorWrapper);
        updatePageButtonVisibility();
    }

    void updatePageButtonVisibility() {
        pageLeftButton.setVisible(parametersPanel->canPageLeft());
        pageRightButton.setVisible(parametersPanel->canPageRight());
        updateEnabledPush2Buttons();
    }

    void aboveScreenButtonPressed(int buttonIndex) {
        if (buttonIndex == 6) {
            pageLeft();
        } else if (buttonIndex == 7) {
            pageRight();
        }
    }

    void belowScreenButtonPressed(int buttonIndex) {}
private:
    std::unique_ptr<ParametersPanel> parametersPanel;
    Label titleLabel;
    ArrowButton pageLeftButton, pageRightButton;

    void pageLeft() {
        parametersPanel->pageLeft();
        updatePageButtonVisibility();
    }

    void pageRight() {
        parametersPanel->pageRight();
        updatePageButtonVisibility();
    }

    void updateEnabledPush2Buttons() {
        push2MidiCommunicator.setAllBelowScreenButtonEnabled(false);

        push2MidiCommunicator.setAboveScreenButtonEnabled(6, parametersPanel->canPageLeft());
        push2MidiCommunicator.setAboveScreenButtonEnabled(7, parametersPanel->canPageRight());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2ProcessorView)
};
