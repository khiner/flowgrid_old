#pragma once

#include "Tracks.h"
#include "ConnectionType.h"
#include "Connection.h"

namespace ConnectionsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CONNECTIONS)
#undef ID
}

struct Connections : public Stateful<Connections> {
    explicit Connections(Tracks &tracks) : tracks(tracks) {}

    static Identifier getIdentifier() { return ConnectionIDs::CONNECTION; }

    ValueTree findDefaultDestinationProcessor(const ValueTree &sourceProcessor, ConnectionType connectionType);

    bool isNodeConnected(AudioProcessorGraph::NodeID nodeId) const;

    Array<ValueTree> getConnectionsForNode(const ValueTree &processor, ConnectionType connectionType,
                                           bool incoming = true, bool outgoing = true,
                                           bool includeCustom = true, bool includeDefault = true);

    ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : state)
            if (Processor::toProcessorGraphConnection(connectionState) == connection)
                return connectionState;
        return {};
    }

    bool anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType);

    static bool isProcessorAnEffect(const ValueTree &processor, ConnectionType connectionType) {
        return (connectionType == audio && Tracks::getNumInputChannelsForProcessor(processor) > 0) ||
               (connectionType == midi && processor[ProcessorIDs::acceptsMidi]);
    }

    static bool isProcessorAProducer(const ValueTree &processor, ConnectionType connectionType) {
        return (connectionType == audio && Tracks::getNumOutputChannelsForProcessor(processor) > 0) ||
               (connectionType == midi && processor[ProcessorIDs::producesMidi]);
    }

private:
    Tracks &tracks;
};
