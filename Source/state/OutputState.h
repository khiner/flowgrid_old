#pragma once

#include <PluginManager.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "State.h"

class OutputState : public State {
public:
    explicit OutputState(StatefulAudioProcessorContainer& audioProcessorContainer, PluginManager& pluginManager)
            : audioProcessorContainer(audioProcessorContainer), pluginManager(pluginManager) {
        output = ValueTree(IDs::OUTPUT);
    }

    ValueTree& getState() override { return output; }

    void loadFromState(const ValueTree& state) override {
        output.copyPropertiesFrom(state, nullptr);
        Utilities::moveAllChildren(state, output, nullptr);
    }

    ValueTree getAudioOutputProcessorState() const {
        return output.getChildWithProperty(IDs::name, pluginManager.getAudioOutputDescription().name);
    }

    void updateWithAudioDeviceSetup(const AudioDeviceManager::AudioDeviceSetup& audioDeviceSetup, UndoManager *undoManager) {
        output.setProperty(IDs::deviceName, audioDeviceSetup.outputDeviceName, undoManager);
    }

private:
    ValueTree output;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    PluginManager &pluginManager;
};
