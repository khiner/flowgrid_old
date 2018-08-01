#pragma once

#include <Identifiers.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"
#include "view/push2/Push2Listener.h"

using Push2 = Push2MidiCommunicator;

class MidiControlHandler {
public:
    explicit MidiControlHandler() {}

    void setPush2Listener(Push2Listener *push2Listener) {
        this->push2Listener = push2Listener;
    }

    // listened to and called on a non-audio thread
    void handleControlMidi(const MidiMessage &midiMessage) {
        if (!midiMessage.isController() || push2Listener == nullptr)
            return;

        const int ccNumber = midiMessage.getControllerNumber();

        if (Push2::isEncoderCcNumber(ccNumber)) {
            float changeAmount = Push2::encoderCcMessageToRotationChange(midiMessage);
            if (ccNumber == Push2::masterKnob) {
                return push2Listener->masterEncoderRotated(changeAmount / 2.0f);
            } else if (Push2::isAboveScreenEncoderCcNumber(ccNumber)) {
                return push2Listener->encoderRotated(ccNumber - Push2::ccNumberForTopKnobIndex(0), changeAmount / 2.0f);
            }
            return;
        }

        if (Push2::isButtonPressControlMessage(midiMessage)) {
            if (Push2::isAboveScreenButtonCcNumber(ccNumber)) {
                return push2Listener->aboveScreenButtonPressed(ccNumber - Push2::topDisplayButton1);
            } else if (Push2::isBelowScreenButtonCcNumber(ccNumber)) {
                return push2Listener->belowScreenButtonPressed(ccNumber - Push2::bottomDisplayButton1);
            } else if (Push2::isArrowButtonCcNumber(ccNumber)) {
                return push2Listener->arrowPressed(Push2::directionForArrowButtonCcNumber(ccNumber));
            }
            switch(ccNumber) {
                case Push2::shift:
                    isShiftHeld = true;
                    return;
                case Push2::undo: return push2Listener->undoButtonPressed(isShiftHeld);
                case Push2::delete_: return push2Listener->deleteButtonPressed();
                case Push2::addTrack: return push2Listener->addTrackButtonPressed();
                case Push2::addDevice: return push2Listener->addDeviceButtonPressed();
                default: return;
            }
        } else if (Push2::isButtonReleaseControlMessage(midiMessage)) {
            switch(ccNumber) {
                case Push2::shift:
                    isShiftHeld = false;
                    return;
                default: return;
            }
        }
    }

private:
    Push2Listener *push2Listener {};
    bool isShiftHeld = false;
};
