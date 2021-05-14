#pragma once

#include "TracksState.h"
#include "ConnectionType.h"

namespace ConnectionsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(CONNECTIONS)
    ID(CONNECTION)
    ID(SOURCE)
    ID(DESTINATION)
    ID(channel)
    ID(isCustomConnection)
#undef ID
}

class ConnectionsState : public Stateful {
public:
    explicit ConnectionsState(TracksState &tracks);

    ValueTree &getState() override { return connections; }

    void loadFromState(const ValueTree &state) override {
        moveAllChildren(state, getState(), nullptr);
    }

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
        for (auto connectionState : connections)
            if (stateToConnection(connectionState) == connection)
                return connectionState;
        return {};
    }

    bool anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType);

    static bool isProcessorAnEffect(const ValueTree &processor, ConnectionType connectionType) {
        return (connectionType == audio && TracksState::getNumInputChannelsForProcessor(processor) > 0) ||
               (connectionType == midi && processor[IDs::acceptsMidi]);
    }

    static bool isProcessorAProducer(const ValueTree &processor, ConnectionType connectionType) {
        return (connectionType == audio && TracksState::getNumOutputChannelsForProcessor(processor) > 0) ||
               (connectionType == midi && processor[IDs::producesMidi]);
    }

private:
    ValueTree connections;
    TracksState &tracks;
};
