#include "OutputState.h"

#include "actions/CreateProcessorAction.h"
#include "processors/MidiOutputProcessor.h"

OutputState::OutputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
    output = ValueTree(IDs::OUTPUT);
    output.addListener(this);
}

OutputState::~OutputState() {
    output.removeListener(this);
}

Array<ValueTree> OutputState::syncOutputDevicesWithDeviceManager() {
    Array<ValueTree> outputProcessorsToDelete;
    for (const auto &outputProcessor : output) {
        if (outputProcessor.hasProperty(IDs::deviceName)) {
            const String &deviceName = outputProcessor[IDs::deviceName];
            if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                outputProcessorsToDelete.add(outputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiOutput::getDevices()) {
        if (deviceManager.isMidiOutputEnabled(deviceName) &&
            !output.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
            auto midiOutputProcessor = CreateProcessorAction::createProcessor(MidiOutputProcessor::getPluginDescription());
            midiOutputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
            output.addChild(midiOutputProcessor, -1, &undoManager);
        }
    }
    return outputProcessorsToDelete;
}

void OutputState::initializeDefault() {
    auto outputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioOutputDescription());
    output.appendChild(outputProcessor, &undoManager);
}
