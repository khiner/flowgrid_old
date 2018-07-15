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

    ~MidiControlHandler() {
        project.removeChangeListener(this);
    }

    // listened to and called on a non-audio thread
    void handleControlMidi(const MidiMessage &midiMessage) {
        if (!midiMessage.isController())
            return;

        const int ccNumber = midiMessage.getControllerNumber();

        if (Push2::isEncoderCcNumber(ccNumber)) {
            int parameterIndex = -1;
            if (ccNumber == Push2::masterKnob) {
                StatefulAudioProcessorWrapper *gainProcessor = audioGraphBuilder.getMasterGainProcessor();
                if (gainProcessor != nullptr) {
                    parameterIndex = 1; // TODO get by name since indexes change
                }
            } else if (Push2::isAboveScreenEncoderCcNumber(ccNumber)) {
                if (currentProcessorToControl != nullptr) {
                    parameterIndex = ccNumber - Push2::ccNumberForTopKnobIndex(0);
                }
            }

            if (parameterIndex != -1) {
                float value = Push2::encoderCcMessageToRotationChange(midiMessage);
                StatefulAudioProcessorWrapper::Parameter *parameter = currentProcessorToControl->getParameterObject(parameterIndex);
                float parameterValue = parameter->getValue();
                parameter->setValue(parameterValue + value / 5.0f); // TODO move manual scaling to param
            }
            return;
        }

        if (Push2::isButtonPressControlMessage(midiMessage)) {
            if (Push2::isAboveScreenButtonCcNumber(ccNumber)) {
                push2Component->aboveScreenButtonPressed(ccNumber - Push2::topDisplayButton1);
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
                    if (push2Component != nullptr)
                        push2Component->openProcessorSelector();
                    return;
                case Push2::up:
                    return project.moveSelectionUp();
                case Push2::down:
                    return project.moveSelectionDown();
                case Push2::left:
                    return project.moveSelectionLeft();
                case Push2::right:
                    return project.moveSelectionRight();
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

    Push2Component *push2Component;
    StatefulAudioProcessorWrapper *currentProcessorToControl;

    bool isShiftHeld = false;

    void itemSelected(const ValueTree& item) override {
        if (item.hasType(IDs::PROCESSOR)) {
            currentProcessorToControl = audioGraphBuilder.getProcessorWrapperForState(item);
        }
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedProcessor()) {
            currentProcessorToControl = nullptr;
        }
    }
};
