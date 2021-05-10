#pragma once

#include "state/ConnectionsState.h"
#include "CreateOrDeleteConnectionsAction.h"

struct CreateConnectionAction : public CreateOrDeleteConnectionsAction {
    CreateConnectionAction(const AudioProcessorGraph::Connection &connection, bool isDefault,
                           ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer);

    bool canAddConnection(const AudioProcessorGraph::Connection &c, StatefulAudioProcessorContainer &audioProcessorContainer);
    bool canAddConnection(AudioProcessorGraph::Node *source, int sourceChannel, AudioProcessorGraph::Node *dest, int destChannel) noexcept;
    bool hasConnectionMatching(const AudioProcessorGraph::Connection &connection);
    static ValueTree stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault);
};
