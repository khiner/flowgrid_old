#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Stateful.h"
#include "InputChannels.h"
#include "OutputChannels.h"
#include "Param.h"
#include "Connection.h"

namespace ProcessorIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR)
ID(id)
ID(name)
ID(processorInitialized)
ID(processorSlot)
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

    bool isInitialized() const { return state[ProcessorIDs::processorInitialized]; }
    void setInitialized(bool initialized) { state.setProperty(ProcessorIDs::processorInitialized, initialized, nullptr); }

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

    float getPluginWindowX() const { return state[ProcessorIDs::pluginWindowX]; }
    void setPluginWindowX(float pluginWindowX) { state.setProperty(ProcessorIDs::pluginWindowX, pluginWindowX, nullptr); }

    float getPluginWindowY() const { return state[ProcessorIDs::pluginWindowY]; }
    void setPluginWindowY(float pluginWindowY) { state.setProperty(ProcessorIDs::pluginWindowY, pluginWindowY, nullptr); }

    static String getId(const ValueTree& state) { return state[ProcessorIDs::id]; }
    static String getProcessorState(const ValueTree& state) { return state[ProcessorIDs::state]; }
    static bool hasProcessorState(const ValueTree& state) { return state.hasProperty(ProcessorIDs::state); }
    static AudioProcessorGraph::NodeID getNodeId(const ValueTree &state) { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ProcessorIDs::nodeId]))) : AudioProcessorGraph::NodeID{}; }
    static bool hasNodeId(const ValueTree& state) { return state.hasProperty(ProcessorIDs::nodeId); }
    static String getName(const ValueTree &state) { return state[ProcessorIDs::name]; }
    static String getDeviceName(const ValueTree &state) { return state[ProcessorIDs::deviceName]; }
    static void setDeviceName(ValueTree &state, const String &deviceName) { state.setProperty(ProcessorIDs::deviceName, deviceName, nullptr); }
    static int getPluginWindowType(const ValueTree &state) { return state[ProcessorIDs::pluginWindowType]; }
    static int getIndex(const ValueTree &state) { return state.getParent().indexOf(state); }
    static void setPluginWindowType(ValueTree &state, int pluginWindowType) { state.setProperty(ProcessorIDs::pluginWindowType, pluginWindowType, nullptr); }

    static AudioProcessorGraph::Connection toProcessorGraphConnection(const ValueTree &state) {
        return {{fg::Connection::getSourceNodeId(state), fg::Connection::getSourceChannel(state)},
                {fg::Connection::getDestinationNodeId(state), fg::Connection::getDestinationChannel(state)}};
    }
};
