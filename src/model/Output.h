#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "PluginManager.h"
#include "StatefulList.h"
#include "Processor.h"

namespace OutputIDs {
#define ID(name) const juce::Identifier name(#name);
ID(OUTPUT)
#undef ID
}

// TODO Should input/output be combined into a single IOState? (Almost all behavior is symmetrical.)
struct Output : public Stateful<Output>, public StatefulList<Processor> {
    Output(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager);
    ~Output() override;

    static Identifier getIdentifier() { return OutputIDs::OUTPUT; }
    bool isChildType(const ValueTree &tree) const override { return Processor::isType(tree); }

    void setDeviceName(const String &deviceName) { state.setProperty(ProcessorIDs::deviceName, deviceName, nullptr); }

    Processor *getProcessorByNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        for (auto *processor : children)
            if (processor->getNodeId() == nodeId)
                return processor;
        return nullptr;
    }

    ValueTree getAudioOutputProcessorState() const {
        return state.getChildWithProperty(ProcessorIDs::name, pluginManager.getAudioOutputDescription().name);
    }

    // Returns output processors to delete
    Array<Processor *> syncOutputDevicesWithDeviceManager();

    Processor *getDefaultOutputProcessorForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) return getChildForState(state.getChildWithProperty(ProcessorIDs::name, pluginManager.getAudioOutputDescription().name));
        return {};
    }

private:
    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    Processor *createNewObject(const ValueTree &tree) override { return new Processor(tree, undoManager, deviceManager); }
    void onChildAdded(Processor *processor) override {
        if (processor->isMidiOutputProcessor() && !deviceManager.isMidiOutputEnabled(processor->getDeviceName()))
            deviceManager.setMidiOutputEnabled(processor->getDeviceName(), true);
    }
    void onChildRemoved(Processor *processor, int oldIndex) override {
        if (processor->isMidiOutputProcessor() && deviceManager.isMidiOutputEnabled(processor->getDeviceName()))
            deviceManager.setMidiOutputEnabled(processor->getDeviceName(), false);
    }

    void onChildChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::deviceName) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.outputDeviceName = processor->getDeviceName();
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
