#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "PluginManager.h"
#include "Stateful.h"
#include "ConnectionType.h"
#include "Identifiers.h"

// TODO Should input/output be combined into a single IOState? (Almost all behavior is symmetrical.)
class OutputState : public Stateful, private ValueTree::Listener {
public:
    OutputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager);

    ~OutputState() override;

    ValueTree &getState() override { return output; }

    void loadFromState(const ValueTree &state) override {
        output.copyPropertiesFrom(state, nullptr);
        moveAllChildren(state, output, nullptr);
    }

    void initializeDefault();

    ValueTree getAudioOutputProcessorState() const {
        return output.getChildWithProperty(IDs::name, pluginManager.getAudioOutputDescription().name);
    }

    // Returns output processors to delete
    Array<ValueTree> syncOutputDevicesWithDeviceManager();

private:
    ValueTree output;
    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[IDs::name] == InternalPluginFormat::getMidiOutputProcessorName() &&
            !deviceManager.isMidiOutputEnabled(child[IDs::deviceName]))
            deviceManager.setMidiOutputEnabled(child[IDs::deviceName], true);
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child[IDs::name] == InternalPluginFormat::getMidiOutputProcessorName() &&
            deviceManager.isMidiOutputEnabled(child[IDs::deviceName]))
            deviceManager.setMidiOutputEnabled(child[IDs::deviceName], false);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::deviceName && tree == output) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.outputDeviceName = tree[IDs::deviceName];
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
