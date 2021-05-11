#pragma once

#include "StatefulAudioProcessorContainer.h"
#include "TracksState.h"

using SAPC = StatefulAudioProcessorContainer;

class ConnectionsState : public Stateful {
public:
    explicit ConnectionsState(StatefulAudioProcessorContainer &audioProcessorContainer, TracksState &tracks);

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
        const auto &sourceState = connectionState.getChildWithName(IDs::SOURCE);
        const auto &destState = connectionState.getChildWithName(IDs::DESTINATION);
        return {{SAPC::getNodeIdForState(sourceState), int(sourceState[IDs::channel])},
                {SAPC::getNodeIdForState(destState),   int(destState[IDs::channel])}};
    }

    ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : connections)
            if (stateToConnection(connectionState) == connection)
                return connectionState;
        return {};
    }

    bool canProcessorDefaultConnectTo(const ValueTree &processor, const ValueTree &otherProcessor, ConnectionType connectionType) const {
        if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor)
            return false;

        return isProcessorAProducer(processor, connectionType) && isProcessorAnEffect(otherProcessor, connectionType);
    }

    bool isProcessorAProducer(const ValueTree &processor, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processor))
            return (connectionType == audio && processorWrapper->processor->getTotalNumOutputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->producesMidi());
        return false;
    }

    bool isProcessorAnEffect(const ValueTree &processor, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processor))
            return (connectionType == audio && processorWrapper->processor->getTotalNumInputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->acceptsMidi());
        return false;
    }

    bool anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType) {
        for (const auto &track : tracks.getState())
            if (!TracksState::isMasterTrack(track))
                for (const auto &processor : TracksState::getProcessorLaneForTrack(track))
                    if (isProcessorAnEffect(processor, connectionType))
                        return true;
        return false;
    }

private:
    ValueTree connections;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    TracksState &tracks;
};
