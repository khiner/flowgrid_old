#pragma once

#include "push2/Push2MidiCommunicator.h"
#include "push2/Push2DisplayBridge.h"
#include "Push2ProcessorView.h"
#include "Push2ProcessorSelector.h"
#include "Push2MixerView.h"
#include "Push2NoteModePadLedManager.h"
#include "state/Project.h"

class Push2Component :
        public Timer,
        public Push2ComponentBase,
        private ChangeListener,
        private ValueTree::Listener {
public:
    explicit Push2Component(ViewState &view, TracksState &tracks, Project &project, Push2MidiCommunicator &push2MidiCommunicator);

    ~Push2Component() override;

    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override;

    void setVisible(bool visible) override;

    void shiftPressed() override;

    void shiftReleased() override;

    void masterEncoderRotated(float changeAmount) override;

    void encoderRotated(int encoderIndex, float changeAmount) override;

    void undoButtonPressed() override;

    void addTrackButtonPressed() override;

    void deleteButtonPressed() override;

    void duplicateButtonPressed() override;

    void addDeviceButtonPressed() override;

    void mixButtonPressed() override;

    void masterButtonPressed() override;

    void aboveScreenButtonPressed(int buttonIndex) override;

    void belowScreenButtonPressed(int buttonIndex) override;

    void arrowPressed(int arrowDirection) override;

    void noteButtonPressed() override { view.setNoteMode(); }

    void sessionButtonPressed() override { view.setSessionMode(); }

    void updateEnabledPush2Buttons() override;

private:
    Project &project;
    ConnectionsState &connections;

    Push2DisplayBridge displayBridge;
    ProcessorGraph &processorGraph;

    Push2ProcessorView processorView;
    Push2ProcessorSelector processorSelector;
    Push2MixerView mixerView;
    Push2NoteModePadLedManager push2NoteModePadLedManager;

    Push2ComponentBase *currentlyViewingChild{};

    void timerCallback() override { drawFrame(); }

    // Render a frame and send it to the Push 2 display (if it's available)
    void drawFrame();

    bool canNavigateInDirection(int direction) const;

    void updatePush2SelectionDependentButtons();

    void updatePush2NoteModePadLedManagerVisibility();

    void updateFocusedProcessor();

    void showChild(Push2ComponentBase *child);

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override;

    void changeListenerCallback(ChangeBroadcaster *source) override;
};
