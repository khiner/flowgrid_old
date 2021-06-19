#include "Input.h"

#include "processors/MidiInputProcessor.h"

Array<Processor *> Input::syncInputDevicesWithDeviceManager() {
    Array<Processor *> inputProcessorsToDelete;
    for (auto *inputProcessor : children) {
        if (inputProcessor->hasDeviceName()) {
            const String &deviceName = inputProcessor->getDeviceName();
            if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                inputProcessorsToDelete.add(inputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiInput::getDevices()) {
        if (deviceManager.isMidiInputEnabled(deviceName) &&
            !state.getChildWithProperty(ProcessorIDs::deviceName, deviceName).isValid()) {
            auto processorState = Processor::initState(MidiInputProcessor::getPluginDescription());
            processorState.setProperty(ProcessorIDs::deviceName, deviceName, nullptr);
            state.appendChild(processorState, &undoManager);
        }
    }
    return inputProcessorsToDelete;
}

void Input::initializeDefault() {
    state.appendChild(Processor::initState(pluginManager.getAudioInputDescription()), &undoManager);
}
