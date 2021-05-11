#include "InputState.h"

#include "actions/DeleteProcessorAction.h"
#include "actions/CreateProcessorAction.h"
#include "processors/MidiInputProcessor.h"

InputState::InputState(TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager, UndoManager &undoManager,
                       AudioDeviceManager &deviceManager)
        : tracks(tracks), connections(connections), audioProcessorContainer(audioProcessorContainer),
          pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
    input = ValueTree(IDs::INPUT);
    input.addListener(this);
    deviceManager.addChangeListener(this);
}

InputState::~InputState() {
    deviceManager.removeChangeListener(this);
    input.removeListener(this);
}

void InputState::syncInputDevicesWithDeviceManager() {
    Array<ValueTree> inputProcessorsToDelete;
    for (const auto &inputProcessor : input) {
        if (inputProcessor.hasProperty(IDs::deviceName)) {
            const String &deviceName = inputProcessor[IDs::deviceName];
            if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                inputProcessorsToDelete.add(inputProcessor);
            }
        }
    }
    for (const auto &inputProcessor : inputProcessorsToDelete) {
        undoManager.perform(new DeleteProcessorAction(inputProcessor, tracks, connections, audioProcessorContainer));
    }
    for (const auto &deviceName : MidiInput::getDevices()) {
        if (deviceManager.isMidiInputEnabled(deviceName) &&
            !input.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
            auto midiInputProcessor = CreateProcessorAction::createProcessor(MidiInputProcessor::getPluginDescription());
            midiInputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
            input.addChild(midiInputProcessor, -1, &undoManager);
        }
    }
}

void InputState::changeListenerCallback(ChangeBroadcaster *source) {
    if (source == &deviceManager) {
        deviceManager.updateEnabledMidiInputsAndOutputs();
        syncInputDevicesWithDeviceManager();
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        // TODO the undomanager behavior around this needs more thinking.
        //  This should be done along with the work to keep disabled IO devices in the graph if they still have connections
        input.setProperty(IDs::deviceName, config.inputDeviceName, nullptr);
    }
}

void InputState::loadFromState(const ValueTree &state) {
    input.copyPropertiesFrom(state, nullptr);
    moveAllChildren(state, input, nullptr);
}

void InputState::initializeDefault() {
    auto inputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioInputDescription());
    input.appendChild(inputProcessor, &undoManager);
}
