#include "InputState.h"

#include "actions/CreateProcessorAction.h"
#include "processors/MidiInputProcessor.h"

InputState::InputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
    input = ValueTree(IDs::INPUT);
    input.addListener(this);
}

InputState::~InputState() {
    input.removeListener(this);
}

Array<ValueTree> InputState::syncInputDevicesWithDeviceManager() {
    Array<ValueTree> inputProcessorsToDelete;
    for (const auto &inputProcessor : input) {
        if (inputProcessor.hasProperty(IDs::deviceName)) {
            const String &deviceName = inputProcessor[IDs::deviceName];
            if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                inputProcessorsToDelete.add(inputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiInput::getDevices()) {
        if (deviceManager.isMidiInputEnabled(deviceName) &&
            !input.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
            auto midiInputProcessor = CreateProcessorAction::createProcessor(MidiInputProcessor::getPluginDescription());
            midiInputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
            input.addChild(midiInputProcessor, -1, &undoManager);
        }
    }
    return inputProcessorsToDelete;
}

void InputState::loadFromState(const ValueTree &state) {
    input.copyPropertiesFrom(state, nullptr);
    moveAllChildren(state, input, nullptr);
}

void InputState::initializeDefault() {
    auto inputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioInputDescription());
    input.appendChild(inputProcessor, &undoManager);
}
