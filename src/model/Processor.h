#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "Stateful.h"
#include "model/Channel.h"
#include "Channels.h"
#include "Param.h"
#include "Connection.h"
#include "processors/InternalPluginFormat.h"
#include "ConnectionType.h"
#include "view/PluginWindowType.h"

namespace ProcessorIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR)
ID(id)
ID(name)
ID(initialized)
ID(slot)
ID(nodeId)
ID(bypassed)
ID(acceptsMidi)
ID(producesMidi)
ID(deviceName)
ID(state)
ID(allowDefaultConnections)
ID(pluginWindowType)
ID(pluginWindowX)
ID(pluginWindowY)
#undef ID
}

// TODO override loadFromState
struct Processor : public Stateful<Processor>, public AudioProcessorListener {
    Processor(UndoManager &undoManager, AudioDeviceManager &deviceManager): Stateful<Processor>(), undoManager(undoManager), deviceManager(deviceManager) {}
    explicit Processor(ValueTree state, UndoManager &undoManager, AudioDeviceManager &deviceManager): Stateful<Processor>(std::move(state)), undoManager(undoManager), deviceManager(deviceManager) {}

    static ValueTree initState(const PluginDescription &description) {
        ValueTree state(getIdentifier());
        state.setProperty(ProcessorIDs::id, description.createIdentifierString(), nullptr);
        state.setProperty(ProcessorIDs::name, description.name, nullptr);
        state.setProperty(ProcessorIDs::allowDefaultConnections, true, nullptr);
        state.setProperty(ProcessorIDs::pluginWindowType, static_cast<int>(PluginWindowType::none), nullptr);
        return state;
    }

    static Identifier getIdentifier() { return ProcessorIDs::PROCESSOR; }

    void loadFromState(const ValueTree &fromState) override;

    String getId() const { return state[ProcessorIDs::id]; }
    int getIndex() const { return state.getParent().indexOf(state); }
    String getName() const { return state[ProcessorIDs::name]; }
    String getDeviceName() const { return state[ProcessorIDs::deviceName]; }
    bool hasDeviceName() const { return state.hasProperty(ProcessorIDs::deviceName); }
    int getSlot() const { return state[ProcessorIDs::slot]; }
    AudioProcessorGraph::NodeID getNodeId() const { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ProcessorIDs::nodeId]))) : AudioProcessorGraph::NodeID{}; }
    bool hasNodeId() const { return state.hasProperty(ProcessorIDs::nodeId); }
    String getProcessorState() const { return state[ProcessorIDs::state]; }
    bool hasProcessorState() const { return state.hasProperty(ProcessorIDs::state); }
    bool isInitialized() const { return state[ProcessorIDs::initialized]; }
    bool isBypassed() const { return state[ProcessorIDs::bypassed]; }
    bool acceptsMidi() const { return state[ProcessorIDs::acceptsMidi]; }
    bool producesMidi() const { return state[ProcessorIDs::producesMidi]; }
    bool allowsDefaultConnections() const { return state[ProcessorIDs::allowDefaultConnections]; }
    int getPluginWindowType() const { return state[ProcessorIDs::pluginWindowType]; }
    int getPluginWindowX() const { return state[ProcessorIDs::pluginWindowX]; }
    int getPluginWindowY() const { return state[ProcessorIDs::pluginWindowY]; }
    bool hasPluginWindowX() const { return state.hasProperty(ProcessorIDs::pluginWindowX); }
    bool hasPluginWindowY() const { return state.hasProperty(ProcessorIDs::pluginWindowY); }
    bool isTrackInputProcessor() const { return getName() == InternalPluginFormat::getTrackInputProcessorName(); }
    bool isTrackOutputProcessor() const { return getName() == InternalPluginFormat::getTrackOutputProcessorName(); }
    bool isAudioInputProcessor() const { return InternalPluginFormat::isAudioInputProcessor(getName()); }
    bool isAudioOutputProcessor() const { return InternalPluginFormat::isAudioOutputProcessor(getName()); }
    bool isMidiInputProcessor() const { return InternalPluginFormat::isMidiInputProcessor(getName()); }
    bool isMidiOutputProcessor() const { return InternalPluginFormat::isMidiOutputProcessor(getName()); }
    bool isTrackIOProcessor() const { return isTrackInputProcessor() || isTrackOutputProcessor(); }
    bool isIoProcessor() const { return InternalPluginFormat::isIoProcessor(state[ProcessorIDs::name]); }
    ValueTree getInputChannels() const { return state.getChildWithProperty(ChannelsIDs::type, int(Channels::Type::input)); }
    ValueTree getOutputChannels() const { return state.getChildWithProperty(ChannelsIDs::type, int(Channels::Type::output)); }
    int getNumInputChannels() const { return getInputChannels().getNumChildren(); }
    int getNumOutputChannels() const { return getOutputChannels().getNumChildren(); }
    bool isProcessorAnEffect(ConnectionType connectionType) const {
        return (connectionType == audio && getNumInputChannels() > 0) || (connectionType == midi && acceptsMidi());
    }
    bool isProcessorAProducer(ConnectionType connectionType) const {
        return (connectionType == audio && getNumOutputChannels() > 0) || (connectionType == midi && producesMidi());
    }

    void setId(const String &id) { state.setProperty(ProcessorIDs::id, id, nullptr); }
    void setNodeId(AudioProcessorGraph::NodeID nodeId) { state.setProperty(ProcessorIDs::nodeId, int(nodeId.uid), nullptr); }
    void setName(const String &name) { state.setProperty(ProcessorIDs::name, name, nullptr); }
    void setDeviceName(const String &deviceName) { state.setProperty(ProcessorIDs::deviceName, deviceName, nullptr); }
    void setSlot(int slot) { state.setProperty(ProcessorIDs::slot, slot, nullptr); }
    void setProcessorState(const String &processorState) { state.setProperty(ProcessorIDs::state, processorState, nullptr); }
    void setInitialized(bool initialized) { state.setProperty(ProcessorIDs::initialized, initialized, nullptr); }
    void setBypassed(bool bypassed, UndoManager *undoManager = nullptr) { state.setProperty(ProcessorIDs::bypassed, bypassed, undoManager); }
    void setAcceptsMidi(bool acceptsMidi) { state.setProperty(ProcessorIDs::acceptsMidi, acceptsMidi, nullptr); }
    void setProducesMidi(bool producesMidi) { state.setProperty(ProcessorIDs::producesMidi, producesMidi, nullptr); }
    void setAllowsDefaultConnections(bool allowsDefaultConnections) { state.setProperty(ProcessorIDs::allowDefaultConnections, allowsDefaultConnections, nullptr); }
    void setPluginWindowType(int pluginWindowType, UndoManager *undoManager = nullptr) { state.setProperty(ProcessorIDs::pluginWindowType, pluginWindowType, undoManager); }
    void setPluginWindowX(int pluginWindowX) { state.setProperty(ProcessorIDs::pluginWindowX, pluginWindowX, nullptr); }
    void setPluginWindowY(int pluginWindowY) { state.setProperty(ProcessorIDs::pluginWindowY, pluginWindowY, nullptr); }

    static String getId(const ValueTree &state) { return state[ProcessorIDs::id]; }
    static int getIndex(const ValueTree &state) { return state.getParent().indexOf(state); }
    static String getName(const ValueTree &state) { return state[ProcessorIDs::name]; }
    static String getDeviceName(const ValueTree &state) { return state[ProcessorIDs::deviceName]; }
    static int getSlot(const ValueTree &state) { return state[ProcessorIDs::slot]; }
    static AudioProcessorGraph::NodeID getNodeId(const ValueTree &state) { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ProcessorIDs::nodeId]))) : AudioProcessorGraph::NodeID{}; }

    static void setId(ValueTree &state, const String &id) { state.setProperty(ProcessorIDs::id, id, nullptr); }
    static void setProcessorState(ValueTree &state, const String &processorState) { state.setProperty(ProcessorIDs::state, processorState, nullptr); }
    static void setName(ValueTree &state, const String &name) { state.setProperty(ProcessorIDs::name, name, nullptr); }
    static void setDeviceName(ValueTree &state, const String &deviceName) { state.setProperty(ProcessorIDs::deviceName, deviceName, nullptr); }

private:
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    struct Channel {
        Channel(AudioProcessor *audioProcessor, AudioDeviceManager &deviceManager, int channelIndex, bool isInput);
        Channel(const ValueTree &state);

        ValueTree toState() const {
            ValueTree state(ChannelIDs::CHANNEL);
            fg::Channel::setChannelIndex(state, channelIndex);
            fg::Channel::setName(state, name);
            fg::Channel::setAbbreviatedName(state, abbreviatedName);
            return state;
        }

        bool operator==(const Channel &other) const noexcept { return name == other.name; }

        int channelIndex;
        String name;
        String abbreviatedName;
    };

public:
    void doAudioProcessorChanged(AudioProcessor *audioProcessor);
    void updateChannels(Array<Channel> &oldChannels, Array<Channel> &newChannels, ValueTree &channelsState);

    void audioProcessorChanged(AudioProcessor *processor, const ChangeDetails &details) override;
    void audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue) override {}
    void audioProcessorParameterChangeGestureBegin(AudioProcessor *processor, int parameterIndex) override {}
    void audioProcessorParameterChangeGestureEnd(AudioProcessor *processor, int parameterIndex) override {}
};
