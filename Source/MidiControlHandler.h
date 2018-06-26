#pragma once

#include <drow/Identifiers.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"
#include "AudioGraphBuilder.h"

using Push2 = Push2MidiCommunicator;

class MidiControlHandler {
public:
    explicit MidiControlHandler(AudioGraphBuilder &audioGraphBuilder, UndoManager &undoManager) : audioGraphBuilder(audioGraphBuilder), undoManager(undoManager) {}

    // listened to and called on a non-audio thread
    void handleControlMidi(const MidiMessage &midiMessage) {
        if (!midiMessage.isController())
            return;

        const int ccNumber = midiMessage.getControllerNumber();

        if (Push2::isAboveScreenEncoderCcNumber(ccNumber)) {
            AudioProcessorParameter *parameter = nullptr;
            if (ccNumber == Push2::masterKnob) {
                parameter = audioGraphBuilder.getMainAudioProcessor()->getParameterByIdentifier(
                        IDs::MASTER_GAIN.toString());
            } else {
                AudioGraphClasses::AudioTrack *selectedTrack = audioGraphBuilder.getMainAudioProcessor()->findSelectedAudioTrack();
                parameter = findParameterAssumingTopKnobCc(selectedTrack, ccNumber);
            }

            if (parameter) {
                float value = Push2::encoderCcMessageToRotationChange(midiMessage);

                auto newValue = parameter->getValue() + value / 5.0f; // todo move manual scaling to param

                if (newValue > 0) {
                    parameter->setValue(newValue);
                }
            }
            return;
        }

        if (Push2::isButtonPressControlMessage(midiMessage)) {
            switch(ccNumber) {
                case Push2::undo:
                    if (isShiftHeld) {
                        return MessageManager::callAsync([&]() { undoManager.redo(); });
                    } else {
                        return MessageManager::callAsync([&]() { undoManager.undo(); });
                    }
                case Push2::shift:
                    isShiftHeld = true;
                    return;
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
    AudioGraphBuilder &audioGraphBuilder;
    UndoManager &undoManager;

    bool isShiftHeld = false;

    AudioProcessorParameter *findParameterAssumingTopKnobCc(AudioGraphClasses::AudioTrack *track, const int ccNumber) {
        if (track == nullptr)
            return nullptr;

        for (int i = 0; i < track->getNumParameters(); i++) {
            if (ccNumber == Push2::ccNumberForTopKnobIndex(i)) {
                return track->getCurrentInstrument()->getParameterByIndex(i);
            }
        }
        return nullptr;
    }
};