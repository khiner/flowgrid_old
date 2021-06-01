#include "DeleteProcessor.h"

#include "view/PluginWindowType.h"

DeleteProcessor::DeleteProcessor(const ValueTree &processorToDelete, Track *track, Connections &connections, ProcessorGraph &processorGraph)
        : track(track->copy()), trackIndex(track->getIndex()),
          processorToDelete(processorToDelete),
          processorIndex(Processor::getIndex(processorToDelete)),
          pluginWindowType(Processor::getPluginWindowType(processorToDelete)),
          disconnectProcessorAction(DisconnectProcessor(connections, processorToDelete, all, true, true, true, true)),
          processorGraph(processorGraph) {}

bool DeleteProcessor::perform() {
    processorGraph.getProcessorWrappers().saveProcessorStateInformationToState(processorToDelete);
    performTemporary();
    Processor::setPluginWindowType(processorToDelete, static_cast<int>(PluginWindowType::none));
    processorGraph.onProcessorDestroyed(processorToDelete);
    return true;
}

bool DeleteProcessor::undo() {
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.getState().appendChild(processorToDelete, nullptr);
    else
        track.getProcessorLane().addChild(processorToDelete, processorIndex, nullptr);
    processorGraph.onProcessorCreated(processorToDelete);
    Processor::setPluginWindowType(processorToDelete, pluginWindowType);
    disconnectProcessorAction.undo();

    return true;
}

bool DeleteProcessor::performTemporary() {
    disconnectProcessorAction.perform();
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.getState().removeChild(processorToDelete, nullptr);
    else
        track.getProcessorLane().removeChild(processorToDelete, nullptr);
    return true;
}

bool DeleteProcessor::undoTemporary() {
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.getState().appendChild(processorToDelete, nullptr);
    else
        track.getProcessorLane().addChild(processorToDelete, processorIndex, nullptr);
    disconnectProcessorAction.undo();
    return true;
}
