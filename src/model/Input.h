#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "push2/Push2MidiDevice.h"
#include "PluginManager.h"
#include "Stateful.h"
#include "StatefulList.h"
#include "Processor.h"
#include "ConnectionType.h"

namespace InputIDs {
#define ID(name) const juce::Identifier name(#name);
ID(INPUT)
#undef ID
}

struct Input : public Stateful<Input>, private StatefulList<Processor> {
    struct Listener {
        virtual void processorAdded(Processor *processor) = 0;
        virtual void processorRemoved(Processor *processor, int oldIndex) = 0;
        virtual void processorOrderChanged() {}
        virtual void processorPropertyChanged(Processor *processor, const Identifier &i) {}
    };

    void addInputListener(Listener *listener) { listeners.add(listener); }
    void removeInputListener(Listener *listener) { listeners.remove(listener); }

    Input(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : StatefulList<Processor>(state), pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
        rebuildObjects();
    }

    ~Input() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return InputIDs::INPUT; }
    bool isChildType(const ValueTree &tree) const override { return Processor::isType(tree); }

    void initializeDefault();

    Processor *getDefaultInputProcessorForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) return getChildForState(state.getChildWithProperty(ProcessorIDs::name, pluginManager.getAudioInputDescription().name));
        if (connectionType == midi) return getChildForState(state.getChildWithProperty(ProcessorIDs::deviceName, Push2MidiDevice::getDeviceName()));
        return {};
    }

    // Returns input processors to delete
    Array<ValueTree> syncInputDevicesWithDeviceManager();

private:
    ListenerList<Listener> listeners;

    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    Processor *createNewObject(const ValueTree &tree) override { return new Processor(tree); }
    void deleteObject(Processor *processor) override { delete processor; }
    void newObjectAdded(Processor *processor) override {
        if (processor->isMidiInputProcessor() && !deviceManager.isMidiInputEnabled(processor->getDeviceName()))
            deviceManager.setMidiInputEnabled(processor->getDeviceName(), true);
        listeners.call([processor](Listener &l) { l.processorAdded(processor); });
    }
    void objectRemoved(Processor *processor, int oldIndex) override {
        if (processor->isMidiInputProcessor() && deviceManager.isMidiInputEnabled(processor->getDeviceName()))
            deviceManager.setMidiInputEnabled(processor->getDeviceName(), false);
        listeners.call([processor, oldIndex](Listener &l) { l.processorRemoved(processor, oldIndex); });
    }
    void objectOrderChanged() override {
        listeners.call([](Listener &l) { l.processorOrderChanged(); });
    }

    void objectChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::deviceName) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.inputDeviceName = processor->getDeviceName();
            deviceManager.setAudioDeviceSetup(config, true);
        }
        listeners.call([processor, i](Listener &l) { l.processorPropertyChanged(processor, i); });
    }
};
