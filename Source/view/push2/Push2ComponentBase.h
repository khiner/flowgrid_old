#pragma once

#include <push2/Push2MidiCommunicator.h>
#include <Project.h>
#include "JuceHeader.h"
#include "Push2Listener.h"
#include "Push2Label.h"

using Push2 = Push2MidiCommunicator;

class Push2ComponentBase : public Component, public Push2Listener {
public:
    Push2ComponentBase(Project& project, Push2MidiCommunicator& push2MidiCommunicator)
            : project(project), push2(push2MidiCommunicator) {}

    // Let inheritors implement only what they need.
    void shiftPressed() override { isShiftHeld = true; }
    void shiftReleased() override { isShiftHeld = false; }
    void masterEncoderRotated(float changeAmount) override {}
    void encoderRotated(int encoderIndex, float changeAmount) override {}
    void undoButtonPressed() override {}
    void addTrackButtonPressed() override {}
    void deleteButtonPressed() override {}
    void addDeviceButtonPressed() override {}
    void masterButtonPressed() override {}
    void aboveScreenButtonPressed(int buttonIndex) override {}
    void belowScreenButtonPressed(int buttonIndex) override {}
    void arrowPressed(Direction direction) override {}

    void setVisible(bool visible) override {
        Component::setVisible(visible);
        if (!visible) {
            for (Direction direction : Push2::directions) {
                push2.disableWhiteLedButton(Push2::ccNumberForArrowButton(direction));
            }
        }
    }

protected:
    const static int NUM_COLUMNS = 8;
    const static int HEADER_FOOTER_HEIGHT = 20;

    bool isShiftHeld { false };

    Project& project;
    Push2MidiCommunicator& push2;
};
