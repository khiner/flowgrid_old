#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Stateful.h"
#include "InputChannels.h"
#include "OutputChannels.h"
#include "Param.h"
#include "Connection.h"
#include "processors/InternalPluginFormat.h"

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
struct Processor : public Stateful<Processor> {
    static Identifier getIdentifier() { return ProcessorIDs::PROCESSOR; }

    void loadFromState(const ValueTree &fromState) override;

    int getIndex() const { return state.getParent().indexOf(state); }

    String getId() const { return state[ProcessorIDs::id]; }
    void setId(const String &id) { state.setProperty(ProcessorIDs::id, id, nullptr); }

    String getName() const { return state[ProcessorIDs::name]; }
    void setName(const String &name) { state.setProperty(ProcessorIDs::name, name, nullptr); }

    String getDeviceName() const { return state[ProcessorIDs::deviceName]; }
    void setDeviceName(const String &deviceName) { state.setProperty(ProcessorIDs::deviceName, deviceName, nullptr); }

    AudioProcessorGraph::NodeID getNodeId() const { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ProcessorIDs::nodeId]))) : AudioProcessorGraph::NodeID{}; }
    bool hasNodeId() const { return state.hasProperty(ProcessorIDs::nodeId); }
    void setDeviceName(const int nodeId) { state.setProperty(ProcessorIDs::nodeId, nodeId, nullptr); }

    String getProcessorState() const { return state[ProcessorIDs::state]; }
    bool hasProcessorState() const { return state.hasProperty(ProcessorIDs::state); }
    void setProcessorState(const String &processorState) { state.setProperty(ProcessorIDs::state, processorState, nullptr); }

    bool isInitialized() const { return state[ProcessorIDs::initialized]; }
    void setInitialized(bool initialized) { state.setProperty(ProcessorIDs::initialized, initialized, nullptr); }

    bool isBypassed() const { return state[ProcessorIDs::bypassed]; }
    void setBypassed(bool bypassed) { state.setProperty(ProcessorIDs::bypassed, bypassed, nullptr); }

    bool acceptsMidi() { return state[ProcessorIDs::acceptsMidi]; }
    void setAcceptsMidi(bool acceptsMidi) { state.setProperty(ProcessorIDs::acceptsMidi, acceptsMidi, nullptr); }

    bool producesMidi() const { return state[ProcessorIDs::producesMidi]; }
    void setProducesMidi(bool producesMidi) { state.setProperty(ProcessorIDs::producesMidi, producesMidi, nullptr); }

    bool allowsDefaultConnections() const { return state[ProcessorIDs::allowDefaultConnections]; }
    void setAllowsDefaultConnections(bool allowsDefaultConnections) { state.setProperty(ProcessorIDs::allowDefaultConnections, allowsDefaultConnections, nullptr); }

    int getPluginWindowType() const { return state[ProcessorIDs::pluginWindowType]; }
    void setPluginWindowType(int pluginWindowType) { state.setProperty(ProcessorIDs::pluginWindowType, pluginWindowType, nullptr); }

    int getPluginWindowX() const { return state[ProcessorIDs::pluginWindowX]; }
    void setPluginWindowX(int pluginWindowX) { state.setProperty(ProcessorIDs::pluginWindowX, pluginWindowX, nullptr); }

    int getPluginWindowY() const { return state[ProcessorIDs::pluginWindowY]; }
    void setPluginWindowY(int pluginWindowY) { state.setProperty(ProcessorIDs::pluginWindowY, pluginWindowY, nullptr); }

    static String getId(const ValueTree& state) { return state[ProcessorIDs::id]; }
    static void setId(ValueTree &state, const String &id) { state.setProperty(ProcessorIDs::id, id, nullptr); }
    static String getProcessorState(const ValueTree& state) { return state[ProcessorIDs::state]; }
    static void setProcessorState(ValueTree &state, const String &processorState) { state.setProperty(ProcessorIDs::state, processorState, nullptr); }
    static bool hasProcessorState(const ValueTree& state) { return state.hasProperty(ProcessorIDs::state); }
    static AudioProcessorGraph::NodeID getNodeId(const ValueTree &state) { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ProcessorIDs::nodeId]))) : AudioProcessorGraph::NodeID{}; }
    static bool hasNodeId(const ValueTree& state) { return state.hasProperty(ProcessorIDs::nodeId); }
    static void setNodeId(ValueTree& state, AudioProcessorGraph::NodeID nodeId) { state.setProperty(ProcessorIDs::nodeId, int(nodeId.uid), nullptr); }
    static String getName(const ValueTree &state) { return state[ProcessorIDs::name]; }
    static void setName(ValueTree &state, const String &name) { state.setProperty(ProcessorIDs::name, name, nullptr); }
    static String getDeviceName(const ValueTree &state) { return state[ProcessorIDs::deviceName]; }
    static void setDeviceName(ValueTree &state, const String &deviceName) { state.setProperty(ProcessorIDs::deviceName, deviceName, nullptr); }
    static int getPluginWindowType(const ValueTree &state) { return state[ProcessorIDs::pluginWindowType]; }
    static int getIndex(const ValueTree &state) { return state.getParent().indexOf(state); }
    static int getSlot(const ValueTree &state) { return state[ProcessorIDs::slot]; }
    static void setSlot(ValueTree &state, int slot) { state.setProperty(ProcessorIDs::slot, slot, nullptr); }
    static bool isBypassed(const ValueTree &state) { return state[ProcessorIDs::bypassed]; }
    static bool producesMidi(const ValueTree &state) { return state[ProcessorIDs::producesMidi]; }
    static bool acceptsMidi(const ValueTree &state) { return state[ProcessorIDs::acceptsMidi]; }
    static int getPluginWindowX(const ValueTree &state) { return state[ProcessorIDs::pluginWindowX]; }
    static int getPluginWindowY(const ValueTree &state) { return state[ProcessorIDs::pluginWindowY]; }
    static void setProducesMidi(ValueTree &state, bool producesMidi) { state.setProperty(ProcessorIDs::producesMidi, producesMidi, nullptr); }
    static void setAcceptsMidi(ValueTree &state, bool acceptsMidi) { state.setProperty(ProcessorIDs::acceptsMidi, acceptsMidi, nullptr); }

    static bool allowsDefaultConnections(const ValueTree& state) { return state[ProcessorIDs::allowDefaultConnections]; }
    static void setAllowsDefaultConnections(ValueTree& state, bool allowsDefaultConnections) { state.setProperty(ProcessorIDs::allowDefaultConnections, allowsDefaultConnections, nullptr); }
    static void setPluginWindowType(ValueTree &state, int pluginWindowType) { state.setProperty(ProcessorIDs::pluginWindowType, pluginWindowType, nullptr); }
    static void setPluginWindowX(ValueTree &state, int pluginWindowX) { state.setProperty(ProcessorIDs::pluginWindowX, pluginWindowX, nullptr); }
    static void setPluginWindowY(ValueTree &state, int pluginWindowY) { state.setProperty(ProcessorIDs::pluginWindowY, pluginWindowY, nullptr); }

    static AudioProcessorGraph::Connection toProcessorGraphConnection(const ValueTree &state) {
        return {{fg::Connection::getSourceNodeId(state), fg::Connection::getSourceChannel(state)},
                {fg::Connection::getDestinationNodeId(state), fg::Connection::getDestinationChannel(state)}};
    }

    static bool isIoProcessor(const ValueTree &processor) { return InternalPluginFormat::isIoProcessor(processor[ProcessorIDs::name]); }

    static bool isTrackInputProcessor(const ValueTree &processor) { return getName(processor) == InternalPluginFormat::getTrackInputProcessorName(); }

    static bool isTrackOutputProcessor(const ValueTree &processor) { return getName(processor) == InternalPluginFormat::getTrackOutputProcessorName(); }

    static bool isMidiInputProcessor(const ValueTree &processor) { return getName(processor) == InternalPluginFormat::getMidiInputProcessorName(); }

    static bool isMidiOutputProcessor(const ValueTree &processor) { return getName(processor) == InternalPluginFormat::getMidiOutputProcessorName(); }

    static bool isTrackIOProcessor(const ValueTree &processor) { return isTrackInputProcessor(processor) || isTrackOutputProcessor(processor); }

    static int getNumInputChannels(const ValueTree &processor) { return processor.getChildWithName(InputChannelsIDs::INPUT_CHANNELS).getNumChildren(); }

    static int getNumOutputChannels(const ValueTree &processor) { return processor.getChildWithName(OutputChannelsIDs::OUTPUT_CHANNELS).getNumChildren(); }
};
