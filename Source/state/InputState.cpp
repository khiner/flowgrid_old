#include "InputState.h"

#include "actions/CreateProcessorAction.h"
#include "processors/MidiInputProcessor.h"

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

void InputState::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && !deviceManager.isMidiInputEnabled(child[TracksStateIDs::deviceName]))
        deviceManager.setMidiInputEnabled(child[TracksStateIDs::deviceName], true);
}

void InputState::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && deviceManager.isMidiInputEnabled(child[TracksStateIDs::deviceName]))
        deviceManager.setMidiInputEnabled(child[TracksStateIDs::deviceName], false);
}

void InputState::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == TracksStateIDs::deviceName && tree == state) {
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        config.inputDeviceName = tree[TracksStateIDs::deviceName];
        deviceManager.setAudioDeviceSetup(config, true);
    }
}
