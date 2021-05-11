#pragma once

#include "PluginManager.h"
#include "StatefulAudioProcessorContainer.h"
#include "Stateful.h"
#include "TracksState.h"
#include "ConnectionsState.h"

// TODO Should input/output be combined into a single IOState? (Almost all behavior is symmetrical.)
class OutputState : public Stateful, private ChangeListener, private ValueTree::Listener {
public:
    OutputState(TracksState &tracks, ConnectionsState &connections,
                StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager,
                UndoManager &undoManager, AudioDeviceManager &deviceManager);

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

private:
    ValueTree output;

    TracksState &tracks;
    ConnectionsState &connections;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void syncOutputDevicesWithDeviceManager();

    void changeListenerCallback(ChangeBroadcaster *source) override;

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
