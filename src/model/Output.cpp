#include "Output.h"

#include "processors/MidiOutputProcessor.h"

Output::Output(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : StatefulList<Processor>(state), pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
    rebuildObjects();
}

Output::~Output() {
    freeObjects();
}

Array<Processor *> Output::syncOutputDevicesWithDeviceManager() {
    Array<Processor *> outputProcessorsToDelete;
    for (auto *outputProcessor : children) {
        if (outputProcessor->hasDeviceName()) {
            const String &deviceName = outputProcessor->getDeviceName();
            if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                outputProcessorsToDelete.add(outputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiOutput::getDevices()) {
        if (deviceManager.isMidiOutputEnabled(deviceName) &&
            !state.getChildWithProperty(ProcessorIDs::deviceName, deviceName).isValid()) {
            auto processorState = Processor::initState(MidiOutputProcessor::getPluginDescription());
            processorState.setProperty(ProcessorIDs::deviceName, deviceName, nullptr);
            state.appendChild(processorState, &undoManager);
        }
    }
    return outputProcessorsToDelete;
}

void Output::initializeDefault() {
    state.appendChild(Processor::initState(pluginManager.getAudioOutputDescription()), &undoManager);
}
