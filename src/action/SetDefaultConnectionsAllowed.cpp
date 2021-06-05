#include "SetDefaultConnectionsAllowed.h"

#include "DisconnectProcessor.h"

SetDefaultConnectionsAllowed::SetDefaultConnectionsAllowed(Processor *processor, bool defaultConnectionsAllowed, Connections &connections)
        : CreateOrDeleteConnections(connections), processor(processor), defaultConnectionsAllowed(defaultConnectionsAllowed) {
    if (!defaultConnectionsAllowed) {
        coalesceWith(DisconnectProcessor(connections, processor, all, true, false, true, true, AudioProcessorGraph::NodeID()));
    }
}

bool SetDefaultConnectionsAllowed::perform() {
    processor->setAllowsDefaultConnections(defaultConnectionsAllowed);
    CreateOrDeleteConnections::perform();
    return true;
}

bool SetDefaultConnectionsAllowed::undo() {
    CreateOrDeleteConnections::perform();
    processor->setAllowsDefaultConnections(!defaultConnectionsAllowed);
    return true;
}
