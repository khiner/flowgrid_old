#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "PluginManager.h"
#include "Stateful.h"
#include "ConnectionType.h"
#include "state/TracksState.h"


namespace OutputStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(OUTPUT)
#undef ID
}

// TODO Should input/output be combined into a single IOState? (Almost all behavior is symmetrical.)
class OutputState : public Stateful, private ValueTree::Listener {
public:
    OutputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager);

    ~OutputState() override;

    ValueTree &getState() override { return state; }

    static bool isType(const ValueTree &state) { return state.hasType(OutputStateIDs::OUTPUT); }

    void loadFromState(const ValueTree &fromState) override;

    void loadFromParentState(const ValueTree &parent) override { loadFromState(parent.getChildWithName(OutputStateIDs::OUTPUT)); }

    void initializeDefault();

    ValueTree getAudioOutputProcessorState() const {
        return state.getChildWithProperty(TracksStateIDs::name, pluginManager.getAudioOutputDescription().name);
    }

    // Returns output processors to delete
    Array<ValueTree> syncOutputDevicesWithDeviceManager();

private:
    ValueTree state;
    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiOutputProcessorName() &&
            !deviceManager.isMidiOutputEnabled(child[TracksStateIDs::deviceName]))
            deviceManager.setMidiOutputEnabled(child[TracksStateIDs::deviceName], true);
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiOutputProcessorName() &&
            deviceManager.isMidiOutputEnabled(child[TracksStateIDs::deviceName]))
            deviceManager.setMidiOutputEnabled(child[TracksStateIDs::deviceName], false);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == TracksStateIDs::deviceName && tree == state) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.outputDeviceName = tree[TracksStateIDs::deviceName];
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
