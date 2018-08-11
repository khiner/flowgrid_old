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
              parameterPageLeftButton("Page left", 0.5, Colours::white),
              parameterPageRightButton("Page right", 0.0, Colours::white) {

        for (int i = 0; i < NUM_COLUMNS; i++) {
            addChildComponent(processorLabels.add(new Push2Label(i, true, push2MidiCommunicator)));
        }

        addChildComponent(escapeProcessorFocusButton);
        addChildComponent(parameterPageLeftButton);
        addChildComponent(parameterPageRightButton);

        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1)).get());

        parameterPageLeftButton.onClick = [this]() { pageLeft(); };
        parameterPageRightButton.onClick = [this]() { pageRight(); };
    }

    void resized() override {
        Push2TrackManagingView::resized();

        auto r = getLocalBounds();
        auto top = r.removeFromTop(HEADER_FOOTER_HEIGHT);
        auto labelWidth = getWidth() / NUM_COLUMNS;
        escapeProcessorFocusButton.setBounds(top.removeFromLeft(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        parameterPageRightButton.setBounds(top.removeFromRight(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        parameterPageLeftButton.setBounds(top.removeFromRight(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
        auto bottom = r.removeFromBottom(HEADER_FOOTER_HEIGHT);
        parametersPanel->setBounds(r);
        
        top = getLocalBounds().removeFromTop(HEADER_FOOTER_HEIGHT);
        for (auto* processorLabel : processorLabels) {
            processorLabel->setBounds(top.removeFromLeft(labelWidth));
        }
    }

    void emptyTrackSelected(const ValueTree& emptyTrack) override {
        Push2TrackManagingView::emptyTrackSelected(emptyTrack);
        parametersPanel->setProcessor(nullptr);
    }
    
    void processorSelected(StatefulAudioProcessorWrapper *const processorWrapper) {
        parametersPanel->setProcessor(processorWrapper);
        updateLabels();
    }

    void updatePageButtonVisibility() {
        escapeProcessorFocusButton.setVisible(processorHasFocus);
        parameterPageLeftButton.setVisible(processorHasFocus && parametersPanel->canPageLeft());
        parameterPageRightButton.setVisible(processorHasFocus && parametersPanel->canPageRight());
        updateEnabledPush2Buttons();
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (!processorHasFocus) {
            if (processorLabels.getUnchecked(buttonIndex)->isSelected()) {
                focusOnProcessor(true);
            } else {
                selectProcessor(buttonIndex);
            }
        } else {
            if (buttonIndex == 0)
                focusOnProcessor(false);
            else if (buttonIndex == NUM_COLUMNS - 2)
                pageLeft();
            else if (buttonIndex == NUM_COLUMNS - 1)
                pageRight();
        }
    }

    void encoderRotated(int encoderIndex, float changeAmount) override {
        jassert(encoderIndex >= 0 && encoderIndex < NUM_COLUMNS);
        if (auto *parameter = parametersPanel->getParameterForIndex(encoderIndex)) {
            parameter->setValue(jlimit(0.0f, 1.0f, parameter->getValue() + changeAmount));
        }
    }

private:
    std::unique_ptr<ParametersPanel> parametersPanel;
    OwnedArray<Push2Label> processorLabels;
    ArrowButton escapeProcessorFocusButton, parameterPageLeftButton, parameterPageRightButton;

    bool processorHasFocus { false };

    void focusOnProcessor(bool focus) {
        processorHasFocus = focus;
        updateLabels();
    }

    void pageLeft() {
        parametersPanel->pageLeft();
        updatePageButtonVisibility();
    }

    void pageRight() {
        parametersPanel->pageRight();
        updatePageButtonVisibility();
    }

    void updateLabels() override {
        Push2TrackManagingView::updateLabels();

        auto selectedTrack = project.getSelectedTrack();
        if (!selectedTrack.isValid())
            return;

        if (processorHasFocus) { // TODO reset when processor changes
            for (auto* label : processorLabels)
                label->setVisible(false);
            updatePageButtonVisibility();
            return;
        }
        updatePageButtonVisibility();

        for (int i = 0; i < processorLabels.size(); i++) {
            auto *label = processorLabels.getUnchecked(i);
            // TODO left/right buttons
            if (i < selectedTrack.getNumChildren()) {
                const auto &processor = selectedTrack.getChild(i);
                if (processor.hasType(IDs::PROCESSOR)) {
                    label->setVisible(true);
                    label->setText(processor[IDs::name], dontSendNotification);
                    label->setSelected(processor[IDs::selected]);
                }
            } else if (i == 0 && selectedTrack.getNumChildren() == 0) {
                label->setVisible(true);
                label->setText("No processors", dontSendNotification);
                label->setSelected(false);
            } else {
                label->setVisible(false);
            }
        }
        valueTreePropertyChanged(selectedTrack, IDs::colour);
    }

    void updateEnabledPush2Buttons() {
        if (processorHasFocus) {
            // TODO update these buttons to manage the push buttons on their own, similar to Push2Label
            push2.setAboveScreenButtonEnabled(0, true); // back button
            push2.setAboveScreenButtonEnabled(NUM_COLUMNS - 2, parametersPanel->canPageLeft());
            push2.setAboveScreenButtonEnabled(NUM_COLUMNS - 1, parametersPanel->canPageRight());
        }
    }

    void selectProcessor(int processorIndex) {
        const auto& selectedTrack = project.getSelectedTrack();
        if (selectedTrack.isValid() && processorIndex < selectedTrack.getNumChildren()) {
            selectedTrack.getChild(processorIndex).setProperty(IDs::selected, true, nullptr);
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        Push2TrackManagingView::valueTreePropertyChanged(tree, i);

        if (tree.hasType(IDs::PROCESSOR)) {
            int processorIndex = tree.getParent().indexOf(tree);
            jassert(processorIndex < processorLabels.size()); // TODO left/right buttons
            if (i == IDs::name) {
                processorLabels.getUnchecked(processorIndex)->setText(tree[IDs::name], dontSendNotification);
            }
        }
    }

    void selectedTrackColourChanged(const Colour& newColour) override {
        for (auto *processorLabel : processorLabels) {
            processorLabel->setMainColour(newColour);
        }
        escapeProcessorFocusButton.setColour(newColour);
        parameterPageLeftButton.setColour(newColour);
        parameterPageRightButton.setColour(newColour);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2ProcessorView)
};
