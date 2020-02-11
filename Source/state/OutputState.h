#pragma once

#include <PluginManager.h>
#include <actions/DeleteProcessorAction.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "Stateful.h"

class OutputState : public Stateful, private ChangeListener {
public:
    OutputState(TracksState &tracks, ConnectionsState &connections,
                         StatefulAudioProcessorContainer& audioProcessorContainer, PluginManager& pluginManager,
                         UndoManager &undoManager, AudioDeviceManager& deviceManager)
            : tracks(tracks), connections(connections),
              audioProcessorContainer(audioProcessorContainer), pluginManager(pluginManager),
              undoManager(undoManager), deviceManager(deviceManager) {
        output = ValueTree(IDs::OUTPUT);
        deviceManager.addChangeListener(this);
    }

    ~OutputState() override {
        deviceManager.removeChangeListener(this);
    }

    ValueTree& getState() override { return output; }

    void loadFromState(const ValueTree& state) override {
        output.copyPropertiesFrom(state, nullptr);
        Utilities::moveAllChildren(state, output, nullptr);
    }

    void initializeDefault() {
        PluginDescription &audioOutputDescription = pluginManager.getAudioOutputDescription();
        ValueTree outputProcessor(IDs::PROCESSOR);
        outputProcessor.setProperty(IDs::id, audioOutputDescription.createIdentifierString(), nullptr);
        outputProcessor.setProperty(IDs::name, audioOutputDescription.name, nullptr);
        outputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
        output.appendChild(outputProcessor, &undoManager);
    }

    ValueTree getAudioOutputProcessorState() const {
        return output.getChildWithProperty(IDs::name, pluginManager.getAudioOutputDescription().name);
    }

private:
    ValueTree output;

    TracksState &tracks;
    ConnectionsState &connections;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    PluginManager &pluginManager;

    UndoManager &undoManager;
    AudioDeviceManager& deviceManager;

    void syncOutputDevicesWithDeviceManager() {
        Array<ValueTree> outputProcessorsToDelete;
        for (const auto& outputProcessor : output) {
            if (outputProcessor.hasProperty(IDs::deviceName)) {
                const String &deviceName = outputProcessor[IDs::deviceName];
                if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                    outputProcessorsToDelete.add(outputProcessor);
                }
            }
        }
        for (const auto& outputProcessor : outputProcessorsToDelete) {
            undoManager.perform(new DeleteProcessorAction(outputProcessor, tracks, connections, audioProcessorContainer));
        }
        for (const auto& deviceName : MidiOutput::getDevices()) {
            if (deviceManager.isMidiOutputEnabled(deviceName) &&
                !output.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiOutputProcessor(IDs::PROCESSOR);
                midiOutputProcessor.setProperty(IDs::id, MidiOutputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiOutputProcessor.setProperty(IDs::name, MidiOutputProcessor::name(), nullptr);
                midiOutputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
                midiOutputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                output.addChild(midiOutputProcessor, -1, &undoManager);
            }
        }
    }

    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &deviceManager) {
            deviceManager.updateEnabledMidiInputsAndOutputs();
            syncOutputDevicesWithDeviceManager();
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            output.setProperty(IDs::deviceName, config.outputDeviceName, &undoManager);
        }
    }
};
