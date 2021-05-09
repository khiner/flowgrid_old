#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include "Push2TrackManagingView.h"
#include "view/processor_editor/ParametersPanel.h"
#include "Push2Label.h"

class Push2ProcessorView : public Push2TrackManagingView {
public:
    explicit Push2ProcessorView(Project &project, Push2MidiCommunicator &push2MidiCommunicator);

    void resized() override;

    void trackSelected(const ValueTree &track) override;
    void processorFocused(StatefulAudioProcessorWrapper *processorWrapper);

    void aboveScreenButtonPressed(int buttonIndex) override;
    void encoderRotated(int encoderIndex, float changeAmount) override;

    void updateEnabledPush2Buttons() override;

private:
    std::unique_ptr<ParametersPanel> parametersPanel;
    OwnedArray<Push2Label> processorLabels{};
    ArrowButton escapeProcessorFocusButton, parameterPageLeftButton, parameterPageRightButton,
            processorPageLeftButton, processorPageRightButton;

    int processorLabelOffset = 0;
    bool processorHasFocus{false};

    static Colour getColourForTrack(const ValueTree &track) {
        return track.isValid() ? TracksState::getTrackColour(track) : Colours::black;
    }

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

    void updatePageButtonVisibility();
    void updateProcessorButtons();
    void updateColours();

    void pageParametersLeft();
    void pageParametersRight();
    void pageProcessorsLeft();
    void pageProcessorsRight();

    bool canPageProcessorsLeft() const { return processorLabelOffset > 0; }

    bool canPageProcessorsRight() const {
        const auto &focusedLane = TracksState::getProcessorLaneForTrack(tracks.getFocusedTrack());
        return focusedLane.getNumChildren() > processorLabelOffset + (canPageProcessorsLeft() ? NUM_COLUMNS - 1 : NUM_COLUMNS);
    }

    void selectProcessor(int processorIndex) {
        const auto &focusedLane = TracksState::getProcessorLaneForTrack(tracks.getFocusedTrack());
        if (focusedLane.isValid() && processorIndex < focusedLane.getNumChildren()) {
            project.selectProcessor(focusedLane.getChild(processorIndex));
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
    void trackColourChanged(const String &trackUuid, const Colour &colour) override;
};
