#pragma once

#include <processors/ProcessorManager.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "StateManager.h"

class InputStateManager : public StateManager {
public:
    explicit InputStateManager(StatefulAudioProcessorContainer& audioProcessorContainer, ProcessorManager& processorManager)
            : audioProcessorContainer(audioProcessorContainer), processorManager(processorManager) {
        input = ValueTree(IDs::INPUT);
    }

    ValueTree& getState() override { return input; }

    void loadFromState(const ValueTree& state) override {
        input.copyPropertiesFrom(state, nullptr);
        Utilities::moveAllChildren(state, input, nullptr);
    }

    ValueTree getAudioInputProcessorState() const {
        return input.getChildWithProperty(IDs::name, processorManager.getAudioInputDescription().name);
    }

    ValueTree getPush2MidiInputProcessor() const {
        return input.getChildWithProperty(IDs::deviceName, push2MidiDeviceName);
    }

    void updateWithAudioDeviceSetup(const AudioDeviceManager::AudioDeviceSetup& audioDeviceSetup, UndoManager *undoManager) {
        input.setProperty(IDs::deviceName, audioDeviceSetup.inputDeviceName, undoManager);
    }

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) {
            return StatefulAudioProcessorContainer::getNodeIdForState(getAudioInputProcessorState());
        } else if (connectionType == midi) {
            return StatefulAudioProcessorContainer::getNodeIdForState(getPush2MidiInputProcessor());
        } else {
            return {};
        }
    }

private:
    ValueTree input;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    ProcessorManager &processorManager;
};
