#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "push2/Push2MidiDevice.h"
#include "PluginManager.h"
#include "Stateful.h"
#include "TracksState.h"
#include "ConnectionType.h"

class InputState : public Stateful, private ValueTree::Listener {
public:
    InputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager);

    ~InputState() override;

    ValueTree &getState() override { return input; }

    void loadFromState(const ValueTree &state) override;

    void initializeDefault();

    ValueTree getAudioInputProcessorState() const {
        return input.getChildWithProperty(TracksStateIDs::name, pluginManager.getAudioInputDescription().name);
    }

    ValueTree getPush2MidiInputProcessor() const {
        return input.getChildWithProperty(TracksStateIDs::deviceName, Push2MidiDevice::getDeviceName());
    }

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) return TracksState::getNodeIdForProcessor(getAudioInputProcessorState());
        if (connectionType == midi) return TracksState::getNodeIdForProcessor(getPush2MidiInputProcessor());
        return {};
    }

    // Returns input processors to delete
    Array<ValueTree> syncInputDevicesWithDeviceManager();

private:
    ValueTree input;
    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && !deviceManager.isMidiInputEnabled(child[TracksStateIDs::deviceName]))
            deviceManager.setMidiInputEnabled(child[TracksStateIDs::deviceName], true);
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && deviceManager.isMidiInputEnabled(child[TracksStateIDs::deviceName]))
            deviceManager.setMidiInputEnabled(child[TracksStateIDs::deviceName], false);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == TracksStateIDs::deviceName && tree == input) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.inputDeviceName = tree[TracksStateIDs::deviceName];
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
