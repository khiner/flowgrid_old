#pragma once

#include <Identifiers.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"
#include "AudioGraphBuilder.h"

using Push2 = Push2MidiCommunicator;

class MidiControlHandler : private ProjectChangeListener {
public:
    explicit MidiControlHandler(Project &project, AudioGraphBuilder &audioGraphBuilder, UndoManager &undoManager) :
            audioGraphBuilder(audioGraphBuilder), undoManager(undoManager) {
        project.addChangeListener(this);
    }

    // listened to and called on a non-audio thread
    void handleControlMidi(const MidiMessage &midiMessage) {
        if (!midiMessage.isController())
            return;

        const int ccNumber = midiMessage.getControllerNumber();

        if (Push2::isEncoderCcNumber(ccNumber)) {
            AudioProcessorParameter *parameter = nullptr;
            if (ccNumber == Push2::masterKnob) {
                parameter = audioGraphBuilder.getMainAudioProcessor()->getParameterByIdentifier(IDs::MASTER_GAIN.toString());
            } else if (Push2::isAboveScreenEncoderCcNumber(ccNumber)) {
                if (currentProcessorToControl != nullptr) {

                    parameter = findParameterAssumingTopKnobCc(currentProcessorToControl, ccNumber);
                }
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
    StatefulAudioProcessor *currentProcessorToControl;

    bool isShiftHeld = false;

    AudioProcessorParameter *findParameterAssumingTopKnobCc(StatefulAudioProcessor *processor, const int ccNumber) {
        if (processor == nullptr)
            return nullptr;

        for (int i = 0; i < processor->getNumParameters(); i++) {
            if (ccNumber == Push2::ccNumberForTopKnobIndex(i)) {
                return processor->getParameterByIndex(i);
            }
        }

        return nullptr;
    }

    void itemSelected(ValueTreeItem *item) override {
        if (item == nullptr) {
        } else if (auto *processor = dynamic_cast<Processor *> (item)) {
            currentProcessorToControl = audioGraphBuilder.getAudioProcessorWithUuid(processor->getState().getProperty(IDs::uuid, processor->getUndoManager()));
        } else if (auto *clip = dynamic_cast<Clip *> (item)) {
        } else if (auto *track = dynamic_cast<Track *> (item)) {
        }
    }
};
