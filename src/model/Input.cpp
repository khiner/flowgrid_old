#include "Input.h"

#include "processors/MidiInputProcessor.h"

Array<ValueTree> Input::syncInputDevicesWithDeviceManager() {
    Array<ValueTree> inputProcessorsToDelete;
    for (const auto &inputProcessor : state) {
        if (inputProcessor.hasProperty(ProcessorIDs::deviceName)) {
            const String &deviceName = Processor::getDeviceName(inputProcessor);
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
