#include "DefaultConnectProcessor.h"

#include "CreateConnection.h"

static const Array<int> &getDefaultConnectionChannels(ConnectionType connectionType) {
    static Array<int> defaultAudioConnectionChannels{0, 1};
    static Array<int> defaultMidiConnectionChannels{AudioProcessorGraph::midiChannelIndex};
    return connectionType == audio ? defaultAudioConnectionChannels : defaultMidiConnectionChannels;
}

DefaultConnectProcessor::DefaultConnectProcessor(const Processor *fromProcessor, AudioProcessorGraph::NodeID toNodeId, ConnectionType connectionType, Connections &connections, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnections(connections) {
    if (fromProcessor != nullptr && toNodeId.isValid()) {
        const auto fromNodeId = fromProcessor->getNodeId();
        const auto &defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
        for (auto channel : defaultConnectionChannels) {
            AudioProcessorGraph::Connection connection = {{fromNodeId, channel}, {toNodeId,   channel}};
            coalesceWith(CreateConnection(connection, true, connections, allProcessors, processorGraph));
        }
    }
}
