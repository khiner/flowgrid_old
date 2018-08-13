#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include "JuceHeader.h"
#include "Push2TrackManagingView.h"
#include "view/processor_editor/ParametersPanel.h"

class Push2MixerView : public Push2TrackManagingView {
public:
    explicit Push2MixerView(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2TrackManagingView(project, push2MidiCommunicator),
              volumesLabel(0, true, push2MidiCommunicator), pansLabel(1, true, push2MidiCommunicator),
              volumeParametersPanel(1), panParametersPanel(1) {
        volumesLabel.setText("Volumes", dontSendNotification);
        pansLabel.setText("Pans", dontSendNotification);
        volumesLabel.setUnderlined(true);
        pansLabel.setUnderlined(true);

        addAndMakeVisible(volumesLabel);
        addAndMakeVisible(pansLabel);
        addChildComponent(volumeParametersPanel);
        addChildComponent(panParametersPanel);
        selectPanel(&volumeParametersPanel);
    }

    void setVisible(bool visible) override {
        Push2TrackManagingView::setVisible(visible);
        volumesLabel.setVisible(visible);
        pansLabel.setVisible(visible);
        if (visible)
            push2.activateWhiteLedButton(Push2::mix);
        else
            push2.enableWhiteLedButton(Push2::mix);
    }

    void resized() override {
        Push2TrackManagingView::resized();

        auto r = getLocalBounds();
        auto top = r.removeFromTop(HEADER_FOOTER_HEIGHT);
        top = getLocalBounds().removeFromTop(HEADER_FOOTER_HEIGHT);
        auto labelWidth = getWidth() / NUM_COLUMNS;
        volumesLabel.setBounds(top.removeFromLeft(labelWidth));
        pansLabel.setBounds(top.removeFromLeft(labelWidth));
        r.removeFromBottom(HEADER_FOOTER_HEIGHT);
        volumeParametersPanel.setBounds(r);
        panParametersPanel.setBounds(r);
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (buttonIndex == 0)
            selectPanel(&volumeParametersPanel);
        else if (buttonIndex == 1)
            selectPanel(&panParametersPanel);
    }

    void encoderRotated(int encoderIndex, float changeAmount) override {
        if (auto *parameter = selectedParametersPanel->getParameterOnCurrentPageAt(encoderIndex)) {
            parameter->setValue(jlimit(0.0f, 1.0f, parameter->getValue() + changeAmount));
        }
    }

private:
    Push2Label volumesLabel, pansLabel;
    ParametersPanel volumeParametersPanel, panParametersPanel;
    ParametersPanel *selectedParametersPanel {};

    void trackAdded(const ValueTree &track) override {
        Push2TrackManagingView::trackAdded(track);
        updateParameters();
    }

    void trackRemoved(const ValueTree &track) override {
        Push2TrackManagingView::trackRemoved(track);
        updateParameters();
    }

    void updateParameters() {
        volumeParametersPanel.clearParameters();
        panParametersPanel.clearParameters();

        for (const auto& track : project.getTracks()) {
            const auto& mixerChannel = project.getMixerChannelProcessorForTrack(track);
            if (mixerChannel.isValid()) { // TODO correct index when tracks are missing MixerChannels
                if (auto* processorWrapper = project.getProcessorWrapperForState(mixerChannel)) {
                    // TODO use param identifiers instead of indexes
                    volumeParametersPanel.addParameter(processorWrapper->getParameter(1));
                    panParametersPanel.addParameter(processorWrapper->getParameter(0));
                }
            }
        }
    }

    void selectedTrackColourChanged(const Colour& newColour) override {}

    void selectPanel(ParametersPanel *panel) {
        if (selectedParametersPanel == panel)
            return;

        if (selectedParametersPanel != nullptr)
            selectedParametersPanel->setVisible(false);

        selectedParametersPanel = panel;
        if (selectedParametersPanel != nullptr)
            selectedParametersPanel->setVisible(true);

        volumesLabel.setSelected(false);
        pansLabel.setSelected(false);

        if (selectedParametersPanel == &volumeParametersPanel)
            volumesLabel.setSelected(true);
        else if (selectedParametersPanel == &panParametersPanel)
            pansLabel.setSelected(true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Push2MixerView)
};
