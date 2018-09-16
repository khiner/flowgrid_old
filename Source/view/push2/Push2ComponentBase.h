#pragma once

#include <push2/Push2MidiCommunicator.h>
#include "Project.h"
#include "JuceHeader.h"
#include "Push2Listener.h"
#include "Push2Label.h"

using Push2 = Push2MidiCommunicator;

class Push2ComponentBase : public Component, public Push2Listener, public Push2Colours::Listener {
public:
    Push2ComponentBase(Project& project, Push2MidiCommunicator& push2)
            : project(project), tracksManager(project.getTracksManager()), viewManager(project.getViewStateManager()),
              push2(push2) {
        push2.getPush2Colours().addListener(this);
    }

    ~Push2ComponentBase() override {
        push2.getPush2Colours().removeListener(this);
    }

    virtual void updateEnabledPush2Buttons() = 0;

    void deviceConnected() override { updateEnabledPush2Buttons(); }

    // Let inheritors implement only what they need.
    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override {}

    void trackColourChanged(const String &trackUuid, const Colour &colour) override {}
    void colourAdded(const Colour& colour, uint8 index) override {}

    void shiftPressed() override { isShiftHeld = true; }
    void shiftReleased() override { isShiftHeld = false; }
    void masterEncoderRotated(float changeAmount) override {}
    void encoderRotated(int encoderIndex, float changeAmount) override {}
    void undoButtonPressed() override {}
    void addTrackButtonPressed() override {}
    void duplicateButtonPressed() override {}
    void deleteButtonPressed() override {}
    void addDeviceButtonPressed() override {}
    void mixButtonPressed() override {}
    void masterButtonPressed() override {}
    void aboveScreenButtonPressed(int buttonIndex) override {}
    void belowScreenButtonPressed(int buttonIndex) override {}
    void arrowPressed(int direction) override {}
    void noteButtonPressed() override {}
    void sessionButtonPressed() override {}

    void setVisible(bool visible) override {
        Component::setVisible(visible);
        updateEnabledPush2Buttons();
    }

protected:
    const static int NUM_COLUMNS = 8;
    const static int HEADER_FOOTER_HEIGHT = 20;

    bool isShiftHeld { false };

    Project& project;
    TracksStateManager& tracksManager;
    ViewStateManager& viewManager;
    Push2MidiCommunicator& push2;
};
