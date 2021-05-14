#pragma once

#include "view/processor_editor/ParametersPanel.h"
#include "Push2TrackManagingView.h"
#include "Push2Label.h"

class Push2MixerView : public Push2TrackManagingView {
public:
    explicit Push2MixerView(ViewState &view, TracksState &tracks, Project &project, Push2MidiCommunicator &push2MidiCommunicator);

    ~Push2MixerView() override;

    void resized() override;

    void aboveScreenButtonPressed(int buttonIndex) override;

    void encoderRotated(int encoderIndex, float changeAmount) override;

    void updateEnabledPush2Buttons() override;

private:
    Push2Label volumesLabel, pansLabel;
    ParametersPanel volumeParametersPanel, panParametersPanel;
    ParametersPanel *selectedParametersPanel{};

    void trackAdded(const ValueTree &track) override;

    void trackRemoved(const ValueTree &track) override;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) override;

    void updateParameters();

    void selectPanel(ParametersPanel *panel);
};
