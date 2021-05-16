#include "OutputState.h"

#include "actions/CreateProcessorAction.h"
#include "processors/MidiOutputProcessor.h"

OutputState::OutputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
    state.addListener(this);
}

OutputState::~OutputState() {
    state.removeListener(this);
}

Array<ValueTree> OutputState::syncOutputDevicesWithDeviceManager() {
    Array<ValueTree> outputProcessorsToDelete;
    for (const auto &outputProcessor : state) {
        if (outputProcessor.hasProperty(TracksStateIDs::deviceName)) {
            const String &deviceName = outputProcessor[TracksStateIDs::deviceName];
            if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                outputProcessorsToDelete.add(outputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiOutput::getDevices()) {
        if (deviceManager.isMidiOutputEnabled(deviceName) &&
            !state.getChildWithProperty(TracksStateIDs::deviceName, deviceName).isValid()) {
            auto midiOutputProcessor = CreateProcessorAction::createProcessor(MidiOutputProcessor::getPluginDescription());
            midiOutputProcessor.setProperty(TracksStateIDs::deviceName, deviceName, nullptr);
            state.addChild(midiOutputProcessor, -1, &undoManager);
        }
    }
    return outputProcessorsToDelete;
}

void OutputState::initializeDefault() {
    auto outputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioOutputDescription());
    state.appendChild(outputProcessor, &undoManager);
}

void OutputState::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiOutputProcessorName() &&
        !deviceManager.isMidiOutputEnabled(child[TracksStateIDs::deviceName]))
        deviceManager.setMidiOutputEnabled(child[TracksStateIDs::deviceName], true);
}

void OutputState::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiOutputProcessorName() &&
        deviceManager.isMidiOutputEnabled(child[TracksStateIDs::deviceName]))
        deviceManager.setMidiOutputEnabled(child[TracksStateIDs::deviceName], false);
}

void OutputState::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == TracksStateIDs::deviceName && tree == state) {
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        config.outputDeviceName = tree[TracksStateIDs::deviceName];
        deviceManager.setAudioDeviceSetup(config, true);
    }
}
