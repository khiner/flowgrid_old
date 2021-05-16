#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "PluginManager.h"
#include "Stateful.h"
#include "ConnectionType.h"
#include "model/Tracks.h"


namespace OutputIDs {
#define ID(name) const juce::Identifier name(#name);
ID(OUTPUT)
#undef ID
}

// TODO Should input/output be combined into a single IOState? (Almost all behavior is symmetrical.)
class Output : public Stateful<Output>, private ValueTree::Listener {
public:
    Output(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager);

    ~Output() override;

    static Identifier getIdentifier() { return OutputIDs::OUTPUT; }

    void initializeDefault();

    ValueTree getAudioOutputProcessorState() const {
        return state.getChildWithProperty(ProcessorIDs::name, pluginManager.getAudioOutputDescription().name);
    }

    // Returns output processors to delete
    Array<ValueTree> syncOutputDevicesWithDeviceManager();

private:
    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
