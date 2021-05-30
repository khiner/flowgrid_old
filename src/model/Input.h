#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "push2/Push2MidiDevice.h"
#include "PluginManager.h"
#include "Stateful.h"
#include "Processor.h"
#include "ConnectionType.h"

namespace InputIDs {
#define ID(name) const juce::Identifier name(#name);
ID(INPUT)
#undef ID
}

struct Input : public Stateful<Input>, private ValueTree::Listener {
    Input(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
        state.addListener(this);
    }

    ~Input() override {
        state.removeListener(this);
    }

    static Identifier getIdentifier() { return InputIDs::INPUT; }

    void initializeDefault();

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) return Processor::getNodeId(state.getChildWithProperty(ProcessorIDs::name, pluginManager.getAudioInputDescription().name));
        if (connectionType == midi) return Processor::getNodeId(state.getChildWithProperty(ProcessorIDs::deviceName, Push2MidiDevice::getDeviceName()));
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
