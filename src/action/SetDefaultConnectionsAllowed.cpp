#include "SetDefaultConnectionsAllowed.h"

#include "DisconnectProcessor.h"

SetDefaultConnectionsAllowed::SetDefaultConnectionsAllowed(const ValueTree &processor, bool defaultConnectionsAllowed, Connections &connections)
        : CreateOrDeleteConnections(connections), processor(processor), defaultConnectionsAllowed(defaultConnectionsAllowed) {
    if (!defaultConnectionsAllowed) {
        coalesceWith(DisconnectProcessor(connections, processor, all, true, false, true, true, AudioProcessorGraph::NodeID()));
    }
}

bool SetDefaultConnectionsAllowed::perform() {
    processor.setProperty(ProcessorIDs::allowDefaultConnections, defaultConnectionsAllowed, nullptr);
    CreateOrDeleteConnections::perform();
    return true;
}

bool SetDefaultConnectionsAllowed::undo() {
    CreateOrDeleteConnections::perform();
    processor.setProperty(ProcessorIDs::allowDefaultConnections, defaultConnectionsAllowed, nullptr);
    return true;
}
