#include "SetDefaultConnectionsAllowedAction.h"

#include "state/Identifiers.h"
#include "DisconnectProcessorAction.h"

SetDefaultConnectionsAllowedAction::SetDefaultConnectionsAllowedAction(const ValueTree &processor, bool defaultConnectionsAllowed, ConnectionsState &connections)
        : CreateOrDeleteConnectionsAction(connections), processor(processor), defaultConnectionsAllowed(defaultConnectionsAllowed) {
    if (!defaultConnectionsAllowed) {
        coalesceWith(DisconnectProcessorAction(connections, processor, all, true, false, true, true, AudioProcessorGraph::NodeID()));
    }
}

bool SetDefaultConnectionsAllowedAction::perform() {
    processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, nullptr);
    CreateOrDeleteConnectionsAction::perform();
    return true;
}

bool SetDefaultConnectionsAllowedAction::undo() {
    CreateOrDeleteConnectionsAction::perform();
    processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, nullptr);
    return true;
}
