#include "OutputState.h"

#include <actions/DeleteProcessorAction.h>
#include <actions/CreateProcessorAction.h>

OutputState::OutputState(TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager, UndoManager &undoManager,
                         AudioDeviceManager &deviceManager)
        : tracks(tracks), connections(connections),
          audioProcessorContainer(audioProcessorContainer), pluginManager(pluginManager),
          undoManager(undoManager), deviceManager(deviceManager) {
    output = ValueTree(IDs::OUTPUT);
    output.addListener(this);
    deviceManager.addChangeListener(this);
}

OutputState::~OutputState() {
    deviceManager.removeChangeListener(this);
    output.removeListener(this);
}

void OutputState::syncOutputDevicesWithDeviceManager() {
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

void OutputState::changeListenerCallback(ChangeBroadcaster *source) {
    if (source == &deviceManager) {
        deviceManager.updateEnabledMidiInputsAndOutputs();
        syncOutputDevicesWithDeviceManager();
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        // TODO the undomanager behavior around this needs more thinking.
        //  This should be done along with the work to keep disabled IO devices in the graph if they still have connections
        output.setProperty(IDs::deviceName, config.outputDeviceName, nullptr);
    }
}

void OutputState::initializeDefault() {
    auto outputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioOutputDescription());
    output.appendChild(outputProcessor, &undoManager);
}
