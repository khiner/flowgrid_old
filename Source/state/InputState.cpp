#include "InputState.h"

#include "actions/CreateProcessorAction.h"
#include "processors/MidiInputProcessor.h"

InputState::InputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
    state = ValueTree(InputStateIDs::INPUT);
    state.addListener(this);
}

InputState::~InputState() {
    state.removeListener(this);
}

void InputState::loadFromState(const ValueTree &fromState) {
    state.copyPropertiesFrom(fromState, nullptr);
    moveAllChildren(fromState, state, nullptr);
}

Array<ValueTree> InputState::syncInputDevicesWithDeviceManager() {
    Array<ValueTree> inputProcessorsToDelete;
    for (const auto &inputProcessor : state) {
        if (inputProcessor.hasProperty(TracksStateIDs::deviceName)) {
            const String &deviceName = inputProcessor[TracksStateIDs::deviceName];
            if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                inputProcessorsToDelete.add(inputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiInput::getDevices()) {
        if (deviceManager.isMidiInputEnabled(deviceName) &&
            !state.getChildWithProperty(TracksStateIDs::deviceName, deviceName).isValid()) {
            auto midiInputProcessor = CreateProcessorAction::createProcessor(MidiInputProcessor::getPluginDescription());
            midiInputProcessor.setProperty(TracksStateIDs::deviceName, deviceName, nullptr);
            state.addChild(midiInputProcessor, -1, &undoManager);
        }
    }
    return inputProcessorsToDelete;
}

void InputState::initializeDefault() {
    auto inputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioInputDescription());
    state.appendChild(inputProcessor, &undoManager);
}
