#pragma once

#include <drow/Identifiers.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"
#include "AudioGraphBuilder.h"

typedef Push2MidiCommunicator Push2;

class MidiControlHandler {
public:
    explicit MidiControlHandler(AudioGraphBuilder &audioGraphBuilder, UndoManager &undoManager) : audioGraphBuilder(audioGraphBuilder), undoManager(undoManager) {}

    // listened to and called on a non-audio thread
    void handleControlMidi(const MidiMessage &midiMessage) {
        if (!midiMessage.isController())
            return;

        const int ccNumber = midiMessage.getControllerNumber();

        if (ccNumber == Push2::getCcNumberForControlLabel(Push2::ControlLabel::undo)) {
            undoManager.undo();
            return;
        }

        AudioProcessorParameter *parameter = nullptr;
        if (ccNumber == Push2::getCcNumberForControlLabel(Push2::ControlLabel::masterKnob)) {
            parameter = audioGraphBuilder.getMainAudioProcessor()->getParameterByIdentifier(IDs::MASTER_GAIN.toString());
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
    }

private:
    AudioGraphBuilder &audioGraphBuilder;
    UndoManager &undoManager;

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