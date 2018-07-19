#pragma once

#include <Identifiers.h>
#include "JuceHeader.h"
#include "push2/Push2MidiCommunicator.h"
#include "ProcessorGraph.h"

using Push2 = Push2MidiCommunicator;

class MidiControlHandler : private ProjectChangeListener {
public:
    explicit MidiControlHandler(Project &project, ProcessorGraph &audioGraphBuilder, UndoManager &undoManager) :
            project(project), audioGraphBuilder(audioGraphBuilder), undoManager(undoManager) {
        project.addChangeListener(this);
    }

    void setPush2Component(Push2Component *push2Component) {
        this->push2Component = push2Component;
    }

    ~MidiControlHandler() override {
        project.removeChangeListener(this);
    }

    // listened to and called on a non-audio thread
    void handleControlMidi(const MidiMessage &midiMessage) {
        if (!midiMessage.isController())
            return;

        const int ccNumber = midiMessage.getControllerNumber();

        if (Push2::isEncoderCcNumber(ccNumber)) {
            int parameterIndex = -1;
            AudioProcessor *processor = nullptr;
            if (ccNumber == Push2::masterKnob) {
                processor = audioGraphBuilder.getMasterGainProcessor()->processor;
                parameterIndex = 1;
            } else if (Push2::isAboveScreenEncoderCcNumber(ccNumber)) {
                if (currentProcessorWrapperToControl != nullptr) {
                    processor = currentProcessorWrapperToControl->processor;
                    parameterIndex = ccNumber - Push2::ccNumberForTopKnobIndex(0);
                }
            }

            if (parameterIndex != -1) {
                if (processor != nullptr) {
                    if (auto *parameter = processor->getParameters()[parameterIndex]) {
                        float value = Push2::encoderCcMessageToRotationChange(midiMessage);
                        parameter->setValueNotifyingHost(jlimit(0.0f, 1.0f, parameter->getValue() + value / 4.0f));
                    }
                }
            }
            return;
        }

        if (Push2::isButtonPressControlMessage(midiMessage)) {
            if (Push2::isAboveScreenButtonCcNumber(ccNumber)) {
                return push2Component->aboveScreenButtonPressed(ccNumber - Push2::topDisplayButton1);
            } else if (Push2::isBelowScreenButtonCcNumber(ccNumber)) {
                return push2Component->belowScreenButtonPressed(ccNumber - Push2::bottomDisplayButton1);
            } else if (Push2::isArrowButtonCcNumber(ccNumber)) {
                return push2Component->arrowPressed(Push2::directionForArrowButtonCcNumber(ccNumber));
            }
            switch(ccNumber) {
                case Push2::shift:
                    isShiftHeld = true;
                    return;
                case Push2::undo:
                    if (isShiftHeld) {
                        undoManager.redo();
                        return;
                    } else {
                        undoManager.undo();
                        return;
                    }
                case Push2::delete_:
                    return project.deleteSelectedItems();
                case Push2::addTrack:
                    project.createAndAddTrack();
                    return;
                case Push2::addDevice:
                    push2Component->openProcessorSelector();
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
    Project &project;
    ProcessorGraph &audioGraphBuilder;
    UndoManager &undoManager;

    Push2Component *push2Component {};
    StatefulAudioProcessorWrapper *currentProcessorWrapperToControl {};

    bool isShiftHeld = false;

    void itemSelected(const ValueTree& item) override {
        currentProcessorWrapperToControl = nullptr;

        if (item.hasType(IDs::PROCESSOR)) {
            currentProcessorWrapperToControl = audioGraphBuilder.getProcessorWrapperForState(item);
        }
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedProcessor()) {
            currentProcessorWrapperToControl = nullptr;
        }
    }
};
