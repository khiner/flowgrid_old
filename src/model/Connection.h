#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Stateful.h"

namespace ConnectionIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CONNECTION)
ID(sourceChannel)
ID(destinationChannel)
ID(sourceNodeId)
ID(destinationNodeId)
ID(isCustomConnection)
#undef ID
}

namespace fg {
struct Connection : public Stateful<Connection> {
    static Identifier getIdentifier() { return ConnectionIDs::CONNECTION; }

    static int getSourceChannel(const ValueTree& state) { return state[ConnectionIDs::sourceChannel]; }
    static void setSourceChannel(ValueTree& state, int channel) { state.setProperty(ConnectionIDs::sourceChannel, channel, nullptr); }
    static int getDestinationChannel(const ValueTree& state) { return state[ConnectionIDs::destinationChannel]; }
    static void setDestinationChannel(ValueTree& state, int channel) { state.setProperty(ConnectionIDs::destinationChannel, channel, nullptr); }

    static AudioProcessorGraph::NodeID getSourceNodeId(const ValueTree& state) { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::sourceNodeId]))) : AudioProcessorGraph::NodeID{}; }
    static void setSourceNodeId(ValueTree& state, AudioProcessorGraph::NodeID sourceNodeId) { state.setProperty(ConnectionIDs::sourceNodeId, int(sourceNodeId.uid), nullptr); }
    static AudioProcessorGraph::NodeID getDestinationNodeId(const ValueTree& state) { return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::destinationNodeId]))) : AudioProcessorGraph::NodeID{}; }
    static void setDestinationNodeId(ValueTree& state, AudioProcessorGraph::NodeID destinationNodeId) { state.setProperty(ConnectionIDs::destinationNodeId, int(destinationNodeId.uid), nullptr); }
};
}
