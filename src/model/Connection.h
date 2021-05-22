#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Stateful.h"

namespace ConnectionIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CONNECTION)
ID(sourceChannelIndex)
ID(destinationChannelIndex)
ID(sourceNodeId)
ID(destinationNodeId)
ID(isCustom)
#undef ID
}

namespace fg {
struct Connection : public Stateful<Connection> {
    static Identifier getIdentifier() { return ConnectionIDs::CONNECTION; }

    static bool isCustom(const ValueTree& state) { return state.hasProperty(ConnectionIDs::isCustom) && state[ConnectionIDs::isCustom]; }
    static void setCustom(ValueTree& state, bool isCustom) { state.setProperty(ConnectionIDs::isCustom, isCustom, nullptr); }

    static int getSourceChannel(const ValueTree& state) { return state[ConnectionIDs::sourceChannelIndex]; }
    static void setSourceChannel(ValueTree& state, int channel) { state.setProperty(ConnectionIDs::sourceChannelIndex, channel, nullptr); }
    static int getDestinationChannel(const ValueTree& state) { return state[ConnectionIDs::destinationChannelIndex]; }
    static void setDestinationChannel(ValueTree& state, int channel) { state.setProperty(ConnectionIDs::destinationChannelIndex, channel, nullptr); }

    static AudioProcessorGraph::NodeID getSourceNodeId(const ValueTree& state) { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::sourceNodeId]))) : AudioProcessorGraph::NodeID{}; }
    static void setSourceNodeId(ValueTree& state, AudioProcessorGraph::NodeID sourceNodeId) { state.setProperty(ConnectionIDs::sourceNodeId, int(sourceNodeId.uid), nullptr); }
    static AudioProcessorGraph::NodeID getDestinationNodeId(const ValueTree& state) { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::destinationNodeId]))) : AudioProcessorGraph::NodeID{}; }
    static void setDestinationNodeId(ValueTree& state, AudioProcessorGraph::NodeID destinationNodeId) { state.setProperty(ConnectionIDs::destinationNodeId, int(destinationNodeId.uid), nullptr); }
};
}
