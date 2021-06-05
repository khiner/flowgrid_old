#include "DeleteProcessor.h"

#include "view/PluginWindowType.h"

DeleteProcessor::DeleteProcessor(Processor processorToDelete, Track *track, Connections &connections, ProcessorGraph &processorGraph)
        : track(track->copy()), trackIndex(track->getIndex()),
          processorToDelete(std::move(processorToDelete)),
          processorIndex(processorToDelete.getIndex()),
          pluginWindowType(processorToDelete.getPluginWindowType()),
          disconnectProcessorAction(DisconnectProcessor(connections, &processorToDelete, all, true, true, true, true)),
          processorGraph(processorGraph) {}

bool DeleteProcessor::perform() {
    processorGraph.getProcessorWrappers().saveProcessorInformationToState(&processorToDelete);
    performTemporary();
    processorToDelete.setPluginWindowType(static_cast<int>(PluginWindowType::none));
    processorGraph.onProcessorDestroyed(&processorToDelete);
    return true;
}

bool DeleteProcessor::undo() {
    if (processorToDelete.isTrackIOProcessor())
        track.getState().appendChild(processorToDelete.getState(), nullptr);
    else
        track.getProcessorLane()->add(processorToDelete.getState(), processorIndex);
    processorGraph.onProcessorCreated(&processorToDelete);
    processorToDelete.setPluginWindowType(pluginWindowType);
    disconnectProcessorAction.undo();

    return true;
}

bool DeleteProcessor::performTemporary() {
    disconnectProcessorAction.perform();
    if (processorToDelete.isTrackIOProcessor())
        track.getState().removeChild(processorToDelete.getState(), nullptr);
    else
        track.getProcessorLane()->remove(processorToDelete.getState());
    return true;
}

bool DeleteProcessor::undoTemporary() {
    if (processorToDelete.isTrackIOProcessor())
        track.getState().appendChild(processorToDelete.getState(), nullptr);
    else
        track.getProcessorLane()->add(processorToDelete.getState(), processorIndex);
    disconnectProcessorAction.undo();
    return true;
}
