#pragma once

#include "JuceHeader.h"

#include <utility>

#include "StatefulAudioProcessorContainer.h"
#include "TracksStateManager.h"

using SAPC = StatefulAudioProcessorContainer;

class ConnectionsStateManager : public StateManager {
public:
    explicit ConnectionsStateManager(StatefulAudioProcessorContainer& audioProcessorContainer,
                                     InputStateManager& inputManager, OutputStateManager& outputManager,
                                     TracksStateManager& tracksManager)
            : audioProcessorContainer(audioProcessorContainer),
              inputManager(inputManager),
              outputManager(outputManager),
              tracksManager(tracksManager) {
        connections = ValueTree(IDs::CONNECTIONS);
    }

    ValueTree& getState() override { return connections; }

    void loadFromState(const ValueTree& state) override {
        Utilities::moveAllChildren(state, getState(), nullptr);
    }

    ValueTree findProcessorToFlowInto(const ValueTree &track, const ValueTree &processor, ConnectionType connectionType) {
        if (!isProcessorAProducer(processor, connectionType))
            return {};

        int siblingDelta = 0;
        ValueTree otherTrack;
        while ((otherTrack = track.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : otherTrack) {
                if (otherProcessor == processor) continue;
                bool isOtherProcessorBelow = int(otherProcessor[IDs::processorSlot]) > int(processor[IDs::processorSlot]) ||
                                             (track != otherTrack && TracksStateManager::isMasterTrack(otherTrack));
                if (!isOtherProcessorBelow) continue;
                if (canProcessorDefaultConnectTo(processor, otherProcessor, connectionType) ||
                    // If a non-effect (another producer) is under this processor in the same track, and no effect processors
                    // to the right have a lower slot, block this producer's output by the other producer,
                    // effectively replacing it.
                    track == otherTrack) {
                    return otherProcessor;
                }
            }
        }

        return outputManager.getAudioOutputProcessorState();
    }

    bool isNodeConnected(AudioProcessorGraph::NodeID nodeId) const {
        for (const auto& connection : connections) {
            if (SAPC::getNodeIdForState(connection.getChildWithName(IDs::SOURCE)) == nodeId)
                return true;
        }

        return false;
    }

    static AudioProcessorGraph::Connection stateToConnection(const ValueTree& connectionState) {
        const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
        const auto& destState = connectionState.getChildWithName(IDs::DESTINATION);
        return {{SAPC::getNodeIdForState(sourceState), int(sourceState[IDs::channel])}, {SAPC::getNodeIdForState(destState), int(destState[IDs::channel])}};
    }

    ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : connections) {
            if (stateToConnection(connectionState) == connection) {
                return connectionState;
            }
        }
        return {};
    }

    // make a snapshot of all the information needed to capture AudioGraph connections and UI positions
    void makeConnectionsSnapshot() {
        connectionsSnapshot = connections.createCopy();
    }

    void restoreConnectionsSnapshot() {
        connections.removeAllChildren(nullptr);
        for (const auto& connection : connectionsSnapshot) {
            connections.addChild(connection, -1, nullptr);
        }
    }

    bool canProcessorDefaultConnectTo(const ValueTree &processor, const ValueTree &otherProcessor, ConnectionType connectionType) const {
        if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor)
            return false;

        return isProcessorAProducer(processor, connectionType) && isProcessorAnEffect(otherProcessor, connectionType);
    }

    bool isProcessorAProducer(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumOutputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->producesMidi());
        }
        return false;
    }

    bool isProcessorAnEffect(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumInputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->acceptsMidi());
        }
        return false;
    }

private:
    ValueTree connections;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    InputStateManager& inputManager;
    OutputStateManager& outputManager;
    TracksStateManager& tracksManager;

    ValueTree connectionsSnapshot;
};
