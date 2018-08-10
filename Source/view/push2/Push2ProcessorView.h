#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include "JuceHeader.h"
#include "Push2ComponentBase.h"
#include "view/processor_editor/ParametersPanel.h"

class Push2ProcessorView : public Push2ComponentBase, private Utilities::ValueTreePropertyChangeListener {
public:
    explicit Push2ProcessorView(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator),
              escapeProcessorFocusButton("Back", 0.5, Colours::white),
              parameterPageLeftButton("Page left", 0.5, Colours::white),
              parameterPageRightButton("Page right", 0.0, Colours::white) {

        project.getState().addListener(this);

        for (int i = 0; i < NUM_COLUMNS; i++) {
            addChildComponent(processorLabels.add(new Push2Label(i, true, push2MidiCommunicator)));
            addChildComponent(trackLabels.add(new Push2Label(i, false, push2MidiCommunicator)));
        }

        addChildComponent(escapeProcessorFocusButton);
        addChildComponent(parameterPageLeftButton);
        addChildComponent(parameterPageRightButton);

        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1)).get());

        parameterPageLeftButton.onClick = [this]() { pageLeft(); };
        parameterPageRightButton.onClick = [this]() { pageRight(); };
    }

    ~Push2ProcessorView() override {
        project.getState().removeListener(this);
    }

    void setVisible(bool visible) override {
        Push2ComponentBase::setVisible(visible);
        push2.enableWhiteLedButton(Push2::master);
        if (visible)
            updateLabels();
    }

    void resized() override {
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
        for (auto* trackLabel : trackLabels) {
            trackLabel->setBounds(bottom.removeFromLeft(labelWidth));
        }
    }

    void emptyTrackSelected(const ValueTree& emptyTrack) {
        parametersPanel->setProcessor(nullptr);
        updateLabels();
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

    void belowScreenButtonPressed(int buttonIndex) override {
        selectTrack(buttonIndex);
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
    OwnedArray<Push2Label> trackLabels;
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

    void updateLabels() {
        auto &selectedTrack = project.getSelectedTrack();
        if (!selectedTrack.isValid())
            return;

        for (int i = 0; i < trackLabels.size(); i++) {
            auto *label = trackLabels.getUnchecked(i);
            // TODO left/right buttons
            if (i < project.getNumNonMasterTracks()) {
                const auto &track = project.getTrack(i);
                label->setVisible(true);
                label->setMainColour(Colour::fromString(track[IDs::colour].toString()));
                label->setText(track[IDs::name], dontSendNotification);
                label->setSelected(track == selectedTrack);
            } else {
                label->setVisible(false);
            }
        }

        if (!project.getMasterTrack().isValid())
            push2.disableWhiteLedButton(Push2::master);
        else if (selectedTrack != project.getMasterTrack()) {
            push2.enableWhiteLedButton(Push2::master);
        } else {
            push2.activateWhiteLedButton(Push2::master);
        }

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

    void selectTrack(int trackIndex) {
        if (trackIndex < project.getNumNonMasterTracks()) {
            project.getTrack(trackIndex).setProperty(IDs::selected, true, nullptr);
        }
    }

    void selectProcessor(int processorIndex) {
        const auto& selectedTrack = project.getSelectedTrack();
        if (selectedTrack.isValid() && processorIndex < selectedTrack.getNumChildren()) {
            selectedTrack.getChild(processorIndex).setProperty(IDs::selected, true, nullptr);
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::MASTER_TRACK) || tree.hasType(IDs::TRACK)) {
            if (i == IDs::name || i == IDs::colour) {
                int trackIndex = tree.getParent().indexOf(tree);
                jassert(trackIndex < trackLabels.size()); // TODO left/right buttons
                if (i == IDs::name) {
                    trackLabels.getUnchecked(trackIndex)->setText(tree[IDs::name], dontSendNotification);
                } else if (i == IDs::colour) {
                    const auto &trackColour = Colour::fromString(tree[IDs::colour].toString());
                    trackLabels.getUnchecked(trackIndex)->setMainColour(trackColour);
                    if (tree == project.getSelectedTrack()) {
                        for (auto *processorLabel : processorLabels) {
                            processorLabel->setMainColour(trackColour);
                        }
                        escapeProcessorFocusButton.setColour(trackColour);
                        parameterPageLeftButton.setColour(trackColour);
                        parameterPageRightButton.setColour(trackColour);
                    }
                }
            }
        } else if (tree.hasType(IDs::PROCESSOR)) {
            int processorIndex = tree.getParent().indexOf(tree);
            jassert(processorIndex < processorLabels.size()); // TODO left/right buttons
            if (i == IDs::name) {
                processorLabels.getUnchecked(processorIndex)->setText(tree[IDs::name], dontSendNotification);
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2ProcessorView)
};
