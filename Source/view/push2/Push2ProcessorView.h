#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include "JuceHeader.h"
#include "Push2TrackManagingView.h"
#include "view/processor_editor/ParametersPanel.h"

class Push2ProcessorView : public Push2TrackManagingView {
public:
    explicit Push2ProcessorView(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2TrackManagingView(project, push2MidiCommunicator),
              escapeProcessorFocusButton("Back", 0.5, Colours::white),
              parameterPageLeftButton("Page parameters left", 0.5, Colours::white),
              parameterPageRightButton("Page parameters right", 0.0, Colours::white),
              processorPageLeftButton("Page processors left", 0.5, Colours::white),
              processorPageRightButton("Page processors right", 0.0, Colours::white) {
        
        for (int i = 0; i < NUM_COLUMNS; i++) {
            addChildComponent(processorLabels.add(new Push2Label(i, true, push2MidiCommunicator)));
        }

        addChildComponent(escapeProcessorFocusButton);
        addChildComponent(parameterPageLeftButton);
        addChildComponent(parameterPageRightButton);
        addChildComponent(processorPageLeftButton);
        addChildComponent(processorPageRightButton);

        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1)).get());
    }

    void resized() override {
        Push2TrackManagingView::resized();

        auto r = getLocalBounds();
        auto top = r.removeFromTop(HEADER_FOOTER_HEIGHT);
        auto labelWidth = getWidth() / NUM_COLUMNS;
        escapeProcessorFocusButton.setBounds(top.removeFromLeft(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        parameterPageRightButton.setBounds(top.removeFromRight(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        parameterPageLeftButton.setBounds(top.removeFromRight(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        processorPageLeftButton.setBounds(escapeProcessorFocusButton.getBounds());
        processorPageRightButton.setBounds(parameterPageRightButton.getBounds());

        auto bottom = r.removeFromBottom(HEADER_FOOTER_HEIGHT);
        parametersPanel->setBounds(r);
        
        top = getLocalBounds().removeFromTop(HEADER_FOOTER_HEIGHT);
        for (auto* processorLabel : processorLabels) {
            processorLabel->setBounds(top.removeFromLeft(labelWidth));
        }
    }

    void trackSelected(const ValueTree &track) override {
        Push2TrackManagingView::trackSelected(track);
        if (track.getNumChildren() == 0) {
            parametersPanel->setProcessorWrapper(nullptr);
        }
    }
    
    void processorSelected(StatefulAudioProcessorWrapper *const processorWrapper) {
        parametersPanel->setProcessorWrapper(processorWrapper);
        if (processorWrapper == nullptr) {
            updateEnabledPush2Buttons();
            return;
        }
        auto processorIndex = processorWrapper->state.getParent().indexOf(processorWrapper->state);
        bool hasPaged = false;
        while ((getButtonIndexForProcessorIndex(processorIndex)) <= 0 && canPageProcessorsLeft()) {
            pageProcessorsLeft();
            hasPaged = true;
        }
        while ((getButtonIndexForProcessorIndex(processorIndex)) >= processorLabels.size() - 1 && canPageProcessorsRight()) {
            pageProcessorsRight();
            hasPaged = true;
        }
        if (!hasPaged)
            updateEnabledPush2Buttons();
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (processorHasFocus) {
            if (buttonIndex == 0)
                focusOnProcessor(false);
            else if (buttonIndex == NUM_COLUMNS - 2)
                pageParametersLeft();
            else if (buttonIndex == NUM_COLUMNS - 1)
                pageParametersRight();
        } else {
            if (buttonIndex == 0 && canPageProcessorsLeft())
                pageProcessorsLeft();
            else if (buttonIndex == NUM_COLUMNS - 1 && canPageProcessorsRight())
                pageProcessorsRight();
            else {
                if (processorLabels.getUnchecked(buttonIndex)->isSelected())
                    focusOnProcessor(true);
                else
                    selectProcessor(getProcessorIndexForButtonIndex(buttonIndex));
            }
        }
    }

    void encoderRotated(int encoderIndex, float changeAmount) override {
        if (auto *parameter = parametersPanel->getParameterOnCurrentPageAt(encoderIndex)) {
            parameter->setValue(jlimit(0.0f, 1.0f, parameter->getValue() + changeAmount));
        }
    }

    void updateEnabledPush2Buttons() override {
        Push2TrackManagingView::updateEnabledPush2Buttons();

        updateProcessorButtons();
        updatePageButtonVisibility();
    }

private:
    std::unique_ptr<ParametersPanel> parametersPanel;
    OwnedArray<Push2Label> processorLabels;
    ArrowButton escapeProcessorFocusButton, parameterPageLeftButton, parameterPageRightButton,
                processorPageLeftButton, processorPageRightButton;

    int processorLabelOffset = 0;
    bool processorHasFocus { false };

    int getProcessorIndexForButtonIndex(int buttonIndex) {
        return buttonIndex + processorLabelOffset - (canPageProcessorsLeft() ? 1 : 0);
    }

    int getButtonIndexForProcessorIndex(int processorIndex) {
        return processorIndex - processorLabelOffset + (canPageProcessorsLeft() ? 1 : 0);
    }

    void focusOnProcessor(bool focus) {
        processorHasFocus = focus;
        updateEnabledPush2Buttons();
    }

    void updatePageButtonVisibility() {
        const auto& selectedTrack = tracksManager.getSelectedTrack();
        Colour trackColour = getColourForTrack(selectedTrack);

        if (processorHasFocus) { // TODO reset when processor changes
            processorPageLeftButton.setVisible(false);
            processorPageRightButton.setVisible(false);

            for (auto *label : processorLabels)
                label->setVisible(false);

            // TODO use Push2Label to unite these two sets of updates into one automagically
            push2.setAboveScreenButtonColour(0, trackColour); // back button
            push2.setAboveScreenButtonColour(NUM_COLUMNS - 2, parametersPanel->canPageLeft() ? trackColour : Colours::black);
            push2.setAboveScreenButtonColour(NUM_COLUMNS - 1, parametersPanel->canPageRight() ? trackColour : Colours::black);

            escapeProcessorFocusButton.setVisible(true);
            parameterPageLeftButton.setVisible(parametersPanel->canPageLeft());
            parameterPageRightButton.setVisible(parametersPanel->canPageRight());
        } else {
            escapeProcessorFocusButton.setVisible(false);
            parameterPageLeftButton.setVisible(false);
            parameterPageRightButton.setVisible(false);

            processorPageLeftButton.setVisible(canPageProcessorsLeft());
            processorPageRightButton.setVisible(canPageProcessorsRight());
            if (canPageProcessorsLeft())
                push2.setAboveScreenButtonColour(0, trackColour);
            if (canPageProcessorsRight())
                push2.setAboveScreenButtonColour(NUM_COLUMNS - 1, trackColour);
        }
    }

    void updateProcessorButtons() {
        const auto& selectedTrack = tracksManager.getSelectedTrack();
        if (processorHasFocus || !selectedTrack.isValid()) { // TODO reset when processor changes
            for (auto* label : processorLabels)
                label->setVisible(false);
        } else {
            for (int buttonIndex = 0; buttonIndex < processorLabels.size(); buttonIndex++) {
                auto *label = processorLabels.getUnchecked(buttonIndex);
                auto processorIndex = getProcessorIndexForButtonIndex(buttonIndex);
                if ((buttonIndex == 0 && canPageProcessorsLeft()) ||
                    (buttonIndex == NUM_COLUMNS - 1 && canPageProcessorsRight())) {
                    label->setVisible(false);
                } else if (processorIndex < selectedTrack.getNumChildren()) {
                    const auto &processor = selectedTrack.getChild(processorIndex);
                    if (processor.hasType(IDs::PROCESSOR)) {
                        label->setVisible(true);
                        label->setText(processor[IDs::name], dontSendNotification);
                        label->setSelected(tracksManager.isProcessorSelected(processor));
                    }
                } else if (buttonIndex == 0 && selectedTrack.getNumChildren() == 0) {
                    label->setVisible(true);
                    label->setText("No processors", dontSendNotification);
                    label->setSelected(false);
                } else {
                    label->setVisible(false);
                }
            }
            updateColours();
        }
    }

    Colour getColourForTrack(const ValueTree& track) {
        return track.isValid() ? tracksManager.getTrackColour(track) : Colours::black;
    }

    void updateColours() {
        const auto& selectedTrack = tracksManager.getSelectedTrack();
        if (selectedTrack.isValid()) {
            const auto& colour = getColourForTrack(selectedTrack);
            for (auto *processorLabel : processorLabels) {
                processorLabel->setMainColour(colour);
            }
            escapeProcessorFocusButton.setColour(colour);
            parameterPageLeftButton.setColour(colour);
            parameterPageRightButton.setColour(colour);
            processorPageLeftButton.setColour(colour);
            processorPageRightButton.setColour(colour);
        }
    }

    void pageParametersLeft() {
        parametersPanel->pageLeft();
        updatePageButtonVisibility();
    }

    void pageParametersRight() {
        parametersPanel->pageRight();
        updatePageButtonVisibility();
    }

    void pageProcessorsLeft() {
        processorLabelOffset -= NUM_COLUMNS - 1;
        updateProcessorButtons();
        updatePageButtonVisibility();
    }

    void pageProcessorsRight() {
        processorLabelOffset += NUM_COLUMNS - 1;
        updateProcessorButtons();
        updatePageButtonVisibility();
    }

    bool canPageProcessorsLeft() const {
        return processorLabelOffset > 0;
    }

    bool canPageProcessorsRight() const {
        const auto& selectedTrack = tracksManager.getSelectedTrack();
        return selectedTrack.getNumChildren() > processorLabelOffset + (canPageProcessorsLeft() ? NUM_COLUMNS - 1 : NUM_COLUMNS);
    }

    void selectProcessor(int processorIndex) {
        const auto& selectedTrack = tracksManager.getSelectedTrack();
        if (selectedTrack.isValid() && processorIndex < selectedTrack.getNumChildren()) {
            selectedTrack.getChild(processorIndex).setProperty(IDs::selected, true, nullptr);
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        Push2TrackManagingView::valueTreePropertyChanged(tree, i);

        if (tree.hasType(IDs::PROCESSOR)) {
            int processorIndex = tree.getParent().indexOf(tree);
            if (i == IDs::name) {
                auto buttonIndex = getButtonIndexForProcessorIndex(processorIndex);
                if (auto* processorLabel = processorLabels[buttonIndex]) {
                    processorLabel->setText(tree[IDs::name], dontSendNotification);
                }
            }
        }
    }

    void trackColourChanged(const String &trackUuid, const Colour &colour) override {
        Push2TrackManagingView::trackColourChanged(trackUuid, colour);
        auto track = tracksManager.findTrackWithUuid(trackUuid);
        if (tracksManager.isTrackSelected(track)) {
            updateColours();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2ProcessorView)
};
