#pragma once

#include <PluginManager.h>
#include <actions/DeleteProcessorAction.h>
#include <actions/CreateProcessorAction.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "Stateful.h"

class OutputState : public Stateful, private ChangeListener, private ValueTree::Listener {
public:
    OutputState(TracksState &tracks, ConnectionsState &connections,
                StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager,
                UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : tracks(tracks), connections(connections),
              audioProcessorContainer(audioProcessorContainer), pluginManager(pluginManager),
              undoManager(undoManager), deviceManager(deviceManager) {
        output = ValueTree(IDs::OUTPUT);
        output.addListener(this);
        deviceManager.addChangeListener(this);
    }

    ~OutputState() override {
        deviceManager.removeChangeListener(this);
        output.removeListener(this);
    }

    ValueTree &getState() override { return output; }

    void loadFromState(const ValueTree &state) override {
        output.copyPropertiesFrom(state, nullptr);
        Utilities::moveAllChildren(state, output, nullptr);
    }

    void initializeDefault() {
        auto outputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioOutputDescription());
        output.appendChild(outputProcessor, &undoManager);
    }

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

    void syncOutputDevicesWithDeviceManager() {
        Array<ValueTree> outputProcessorsToDelete;
        for (const auto &outputProcessor : output) {
            if (outputProcessor.hasProperty(IDs::deviceName)) {
                const String &deviceName = outputProcessor[IDs::deviceName];
                if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                    outputProcessorsToDelete.add(outputProcessor);
                }
            }
        }
        for (const auto &outputProcessor : outputProcessorsToDelete) {
            undoManager.perform(new DeleteProcessorAction(outputProcessor, tracks, connections, audioProcessorContainer));
        }
        for (const auto &deviceName : MidiOutput::getDevices()) {
            if (deviceManager.isMidiOutputEnabled(deviceName) &&
                !output.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                auto midiOutputProcessor = CreateProcessorAction::createProcessor(MidiOutputProcessor::getPluginDescription());
                midiOutputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                output.addChild(midiOutputProcessor, -1, &undoManager);
            }
        }
    }

    void changeListenerCallback(ChangeBroadcaster *source) override {
        if (source == &deviceManager) {
            deviceManager.updateEnabledMidiInputsAndOutputs();
            syncOutputDevicesWithDeviceManager();
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            output.setProperty(IDs::deviceName, config.outputDeviceName, &undoManager);
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[IDs::name] == MidiOutputProcessor::name() && !deviceManager.isMidiOutputEnabled(child[IDs::deviceName]))
            deviceManager.setMidiOutputEnabled(child[IDs::deviceName], true);
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child[IDs::name] == MidiOutputProcessor::name() && deviceManager.isMidiOutputEnabled(child[IDs::deviceName]))
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
