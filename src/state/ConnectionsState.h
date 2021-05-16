#pragma once

#include "TracksState.h"
#include "ConnectionType.h"
#include "ConnectionState.h"

namespace ConnectionsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(CONNECTIONS)
    ID(SOURCE)
    ID(DESTINATION)
    ID(channel)
    ID(isCustomConnection)
#undef ID
}

class ConnectionsState : public Stateful<ConnectionsState> {
public:
    explicit ConnectionsState(TracksState &tracks) : tracks(tracks) {}

    static Identifier getIdentifier() { return ConnectionStateIDs::CONNECTION; }

    ValueTree findDefaultDestinationProcessor(const ValueTree &sourceProcessor, ConnectionType connectionType);

    bool isNodeConnected(AudioProcessorGraph::NodeID nodeId) const;

    Array<ValueTree> getConnectionsForNode(const ValueTree &processor, ConnectionType connectionType,
                                           bool incoming = true, bool outgoing = true,
                                           bool includeCustom = true, bool includeDefault = true);

    static AudioProcessorGraph::Connection stateToConnection(const ValueTree &connectionState) {
        const auto &sourceState = connectionState.getChildWithName(ConnectionsStateIDs::SOURCE);
        const auto &destState = connectionState.getChildWithName(ConnectionsStateIDs::DESTINATION);
        return {{TracksState::getNodeIdForProcessor(sourceState), int(sourceState[ConnectionsStateIDs::channel])},
                {TracksState::getNodeIdForProcessor(destState),   int(destState[ConnectionsStateIDs::channel])}};
    }

    ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : state)
            if (stateToConnection(connectionState) == connection)
                return connectionState;
        return {};
    }

    bool anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType);

    static bool isProcessorAnEffect(const ValueTree &processor, ConnectionType connectionType) {
        return (connectionType == audio && TracksState::getNumInputChannelsForProcessor(processor) > 0) ||
               (connectionType == midi && processor[ProcessorStateIDs::acceptsMidi]);
    }

    static bool isProcessorAProducer(const ValueTree &processor, ConnectionType connectionType) {
        return (connectionType == audio && TracksState::getNumOutputChannelsForProcessor(processor) > 0) ||
               (connectionType == midi && processor[ProcessorStateIDs::producesMidi]);
    }

private:
    TracksState &tracks;
};
