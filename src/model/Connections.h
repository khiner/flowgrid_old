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

    static Identifier getIdentifier() { return ConnectionsIDs::CONNECTIONS; }

    Processor *findDefaultDestinationProcessor(const Processor *sourceProcessor, ConnectionType connectionType);

    bool isNodeConnected(AudioProcessorGraph::NodeID nodeId) const;

    Array<ValueTree> getConnectionsForNode(const Processor *processor, ConnectionType connectionType,
                                           bool incoming = true, bool outgoing = true,
                                           bool includeCustom = true, bool includeDefault = true);

    ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : state)
            if (Processor::toProcessorGraphConnection(connectionState) == connection)
                return connectionState;
        return {};
    }

private:
    Tracks &tracks;
};
