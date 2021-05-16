#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "push2/Push2MidiDevice.h"
#include "PluginManager.h"
#include "Stateful.h"
#include "Tracks.h"
#include "ConnectionType.h"

namespace InputIDs {
#define ID(name) const juce::Identifier name(#name);
ID(INPUT)
#undef ID
}

class Input : public Stateful<Input>, private ValueTree::Listener {
public:
    Input(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
        state.addListener(this);
    }

    ~Input() override {
        state.removeListener(this);
    }

    static Identifier getIdentifier() { return InputIDs::INPUT; }

    void initializeDefault();

    ValueTree getAudioInputProcessorState() const {
        return state.getChildWithProperty(ProcessorIDs::name, pluginManager.getAudioInputDescription().name);
    }

    ValueTree getPush2MidiInputProcessor() const {
        return state.getChildWithProperty(ProcessorIDs::deviceName, Push2MidiDevice::getDeviceName());
    }

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) return Tracks::getNodeIdForProcessor(getAudioInputProcessorState());
        if (connectionType == midi) return Tracks::getNodeIdForProcessor(getPush2MidiInputProcessor());
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
