#pragma once

#include <processors/ProcessorManager.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "StateManager.h"

class OutputStateManager : public StateManager {
public:
    explicit OutputStateManager(StatefulAudioProcessorContainer& audioProcessorContainer, ProcessorManager& processorManager)
            : audioProcessorContainer(audioProcessorContainer), processorManager(processorManager) {
        output = ValueTree(IDs::OUTPUT);
    }

    ValueTree& getState() override { return output; }

    void loadFromState(const ValueTree& state) override {
        output.copyPropertiesFrom(state, nullptr);
        Utilities::moveAllChildren(state, output, nullptr);
    }

    ValueTree getAudioOutputProcessorState() const {
        return output.getChildWithProperty(IDs::name, processorManager.getAudioOutputDescription().name);
    }

    void updateWithAudioDeviceSetup(const AudioDeviceManager::AudioDeviceSetup& audioDeviceSetup, UndoManager *undoManager) {
        output.setProperty(IDs::deviceName, audioDeviceSetup.outputDeviceName, undoManager);
    }

private:
    ValueTree output;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    ProcessorManager &processorManager;
};
