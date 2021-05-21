#include "Input.h"

#include "action/CreateProcessor.h"
#include "processors/MidiInputProcessor.h"

Array<ValueTree> Input::syncInputDevicesWithDeviceManager() {
    Array<ValueTree> inputProcessorsToDelete;
    for (const auto &inputProcessor : state) {
        if (inputProcessor.hasProperty(ProcessorIDs::deviceName)) {
            const String &deviceName = inputProcessor[ProcessorIDs::deviceName];
            if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                inputProcessorsToDelete.add(inputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiInput::getDevices()) {
        if (deviceManager.isMidiInputEnabled(deviceName) &&
            !state.getChildWithProperty(ProcessorIDs::deviceName, deviceName).isValid()) {
            auto midiInputProcessor = CreateProcessor::createProcessor(MidiInputProcessor::getPluginDescription());
            Processor::setDeviceName(midiInputProcessor, deviceName);
            state.addChild(midiInputProcessor, -1, &undoManager);
        }
    }
    return inputProcessorsToDelete;
}

void Input::initializeDefault() {
    auto inputProcessor = CreateProcessor::createProcessor(pluginManager.getAudioInputDescription());
    state.appendChild(inputProcessor, &undoManager);
}

void Input::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (child[ProcessorIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && !deviceManager.isMidiInputEnabled(child[ProcessorIDs::deviceName]))
        deviceManager.setMidiInputEnabled(child[ProcessorIDs::deviceName], true);
}

void Input::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (child[ProcessorIDs::name] == InternalPluginFormat::getMidiInputProcessorName() && deviceManager.isMidiInputEnabled(child[ProcessorIDs::deviceName]))
        deviceManager.setMidiInputEnabled(child[ProcessorIDs::deviceName], false);
}

void Input::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ProcessorIDs::deviceName && tree == state) {
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        config.inputDeviceName = tree[ProcessorIDs::deviceName];
        deviceManager.setAudioDeviceSetup(config, true);
    }
}
