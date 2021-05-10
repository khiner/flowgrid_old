#include "DefaultConnectProcessorAction.h"

#include "CreateConnectionAction.h"

static const Array<int> &getDefaultConnectionChannels(ConnectionType connectionType) {
    static Array<int> defaultAudioConnectionChannels{0, 1};
    static Array<int> defaultMidiConnectionChannels{AudioProcessorGraph::midiChannelIndex};
    return connectionType == audio ? defaultAudioConnectionChannels : defaultMidiConnectionChannels;
}

DefaultConnectProcessorAction::DefaultConnectProcessorAction(const ValueTree &fromProcessor, AudioProcessorGraph::NodeID toNodeId, ConnectionType connectionType, ConnectionsState &connections,
                                                             StatefulAudioProcessorContainer &audioProcessorContainer)
        : CreateOrDeleteConnectionsAction(connections) {
    const auto fromNodeId = StatefulAudioProcessorContainer::getNodeIdForState(fromProcessor);
    if (fromProcessor.isValid() && toNodeId.isValid()) {
        const auto &defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
        for (auto channel : defaultConnectionChannels) {
            AudioProcessorGraph::Connection connection = {{fromNodeId, channel},
                                                          {toNodeId,   channel}};
            coalesceWith(CreateConnectionAction(connection, true, connections, audioProcessorContainer));
        }
    }
}
