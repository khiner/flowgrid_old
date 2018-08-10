#pragma once

#include <push2/Push2MidiCommunicator.h>
#include <Project.h>
#include "JuceHeader.h"
#include "Push2Listener.h"

class Push2ComponentBase : public Component, public Push2Listener {
public:
    Push2ComponentBase(Project& project, Push2MidiCommunicator& push2MidiCommunicator)
            : project(project), push2MidiCommunicator(push2MidiCommunicator) {}

    // Let inheritors implement only what they need.
    void masterEncoderRotated(float changeAmount) override {}
    void encoderRotated(int encoderIndex, float changeAmount) override {}
    void undoButtonPressed(bool shiftHeld) override {}
    void addTrackButtonPressed(bool shiftHeld) override {}
    void deleteButtonPressed() override {}
    void addDeviceButtonPressed() override {}
    void aboveScreenButtonPressed(int buttonIndex) override {}
    void belowScreenButtonPressed(int buttonIndex) override {}
    void arrowPressed(Direction direction) override {}

    void setVisible(bool visible) override {
        Component::setVisible(visible);
        if (!visible) {
            push2MidiCommunicator.setAllAboveScreenButtonEnabled(false);
            push2MidiCommunicator.setAllBelowScreenButtonEnabled(false);
            push2MidiCommunicator.setAllArrowButtonsEnabled(false);
        }
    }

protected:
    Project& project;
    Push2MidiCommunicator& push2MidiCommunicator;
};
