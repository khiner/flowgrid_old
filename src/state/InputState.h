#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "push2/Push2MidiDevice.h"
#include "PluginManager.h"
#include "Stateful.h"
#include "TracksState.h"
#include "ConnectionType.h"

namespace InputStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(INPUT)
#undef ID
}

class InputState : public Stateful<InputState>, private ValueTree::Listener {
public:
    InputState(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
        state.addListener(this);
    }

    ~InputState() override {
        state.removeListener(this);
    }

    static Identifier getIdentifier() { return InputStateIDs::INPUT; }

    void initializeDefault();

    ValueTree getAudioInputProcessorState() const {
        return state.getChildWithProperty(TrackStateIDs::name, pluginManager.getAudioInputDescription().name);
    }

    ValueTree getPush2MidiInputProcessor() const {
        return state.getChildWithProperty(ProcessorStateIDs::deviceName, Push2MidiDevice::getDeviceName());
    }

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) return TracksState::getNodeIdForProcessor(getAudioInputProcessorState());
        if (connectionType == midi) return TracksState::getNodeIdForProcessor(getPush2MidiInputProcessor());
        return {};
    }

    // Returns input processors to delete
    Array<ValueTree> syncInputDevicesWithDeviceManager();

private:
    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
