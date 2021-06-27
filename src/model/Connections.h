#pragma once

#include "Tracks.h"
#include "ConnectionType.h"
#include "Connection.h"

namespace ConnectionsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CONNECTIONS)
#undef ID
}

struct Connections : public Stateful<Connections>, StatefulList<fg::Connection> {
    explicit Connections(Tracks &tracks) : StatefulList<fg::Connection>(state), tracks(tracks) {}

    ~Connections() override { freeObjects(); }

    static Identifier getIdentifier() { return ConnectionsIDs::CONNECTIONS; }
    bool isChildType(const ValueTree &tree) const override { return fg::Connection::isType(tree); }

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

protected:
    fg::Connection *createNewObject(const ValueTree &tree) override { return new fg::Connection(tree); }

private:
    Tracks &tracks;
};
