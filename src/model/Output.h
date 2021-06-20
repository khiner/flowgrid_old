#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "PluginManager.h"
#include "Stateful.h"
#include "StatefulList.h"
#include "Processor.h"

namespace OutputIDs {
#define ID(name) const juce::Identifier name(#name);
ID(OUTPUT)
#undef ID
}

// TODO Should input/output be combined into a single IOState? (Almost all behavior is symmetrical.)
struct Output : public Stateful<Output>, public StatefulList<Processor> {
    struct Listener {
        virtual void processorAdded(Processor *) {}
        virtual void processorRemoved(Processor *, int oldIndex)  {}
        virtual void processorOrderChanged() {}
        virtual void processorPropertyChanged(Processor *, const Identifier &) {}
    };

    void addOutputListener(Listener *listener) { listeners.add(listener); }
    void removeOutputListener(Listener *listener) { listeners.remove(listener); }

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
    ListenerList<Listener> listeners;

    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    Processor *createNewObject(const ValueTree &tree) override { return new Processor(tree, undoManager, deviceManager); }
    void deleteObject(Processor *processor) override { delete processor; }
    void newObjectAdded(Processor *processor) override {
        if (processor->isMidiOutputProcessor() && !deviceManager.isMidiOutputEnabled(processor->getDeviceName()))
            deviceManager.setMidiOutputEnabled(processor->getDeviceName(), true);
        listeners.call([processor](Listener &l) { l.processorAdded(processor); });
    }
    void objectRemoved(Processor *processor, int oldIndex) override {
        if (processor->isMidiOutputProcessor() && deviceManager.isMidiOutputEnabled(processor->getDeviceName()))
            deviceManager.setMidiOutputEnabled(processor->getDeviceName(), false);
        listeners.call([processor, oldIndex](Listener &l) { l.processorRemoved(processor, oldIndex); });
    }
    void objectOrderChanged() override {
        listeners.call([](Listener &l) { l.processorOrderChanged(); });
    }

    void objectChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::deviceName) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.outputDeviceName = processor->getDeviceName();
            deviceManager.setAudioDeviceSetup(config, true);
        }
        listeners.call([processor, i](Listener &l) { l.processorPropertyChanged(processor, i); });
    }
};
