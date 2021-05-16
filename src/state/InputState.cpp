#include "InputState.h"

#include "actions/CreateProcessorAction.h"
#include "processors/MidiInputProcessor.h"

Array<ValueTree> InputState::syncInputDevicesWithDeviceManager() {
    Array<ValueTree> inputProcessorsToDelete;
    for (const auto &inputProcessor : state) {
        if (inputProcessor.hasProperty(ProcessorStateIDs::deviceName)) {
            const String &deviceName = inputProcessor[ProcessorStateIDs::deviceName];
            if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                inputProcessorsToDelete.add(inputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiInput::getDevices()) {
        if (deviceManager.isMidiInputEnabled(deviceName) &&
            !state.getChildWithProperty(ProcessorStateIDs::deviceName, deviceName).isValid()) {
            auto midiInputProcessor = CreateProcessorAction::createProcessor(MidiInputProcessor::getPluginDescription());
            midiInputProcessor.setProperty(ProcessorStateIDs::deviceName, deviceName, nullptr);
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
    if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && !deviceManager.isMidiInputEnabled(child[ProcessorStateIDs::deviceName]))
        deviceManager.setMidiInputEnabled(child[ProcessorStateIDs::deviceName], true);
}

void InputState::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (child[TracksStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && deviceManager.isMidiInputEnabled(child[ProcessorStateIDs::deviceName]))
        deviceManager.setMidiInputEnabled(child[ProcessorStateIDs::deviceName], false);
}

void InputState::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ProcessorStateIDs::deviceName && tree == state) {
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        config.inputDeviceName = tree[ProcessorStateIDs::deviceName];
        deviceManager.setAudioDeviceSetup(config, true);
    }
}
