#include "DeleteProcessor.h"

#include "view/PluginWindow.h"

DeleteProcessor::DeleteProcessor(const ValueTree &processorToDelete, Connections &connections, ProcessorGraph &processorGraph)
        : track(Track::getTrackForProcessor(processorToDelete)), trackIndex(Track::getIndex(track)),
          processorToDelete(processorToDelete),
          processorIndex(Processor::getIndex(processorToDelete)),
          pluginWindowType(Processor::getPluginWindowType(processorToDelete)),
          disconnectProcessorAction(DisconnectProcessor(connections, processorToDelete, all, true, true, true, true)),
          processorGraph(processorGraph) {}

bool DeleteProcessor::perform() {
    processorGraph.saveProcessorStateInformationToState(processorToDelete);
    performTemporary();
    Processor::setPluginWindowType(processorToDelete, static_cast<int>(PluginWindow::Type::none));
    processorGraph.onProcessorDestroyed(processorToDelete);
    return true;
}

bool DeleteProcessor::undo() {
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        Track::getProcessorLane(track).addChild(processorToDelete, processorIndex, nullptr);
    processorGraph.onProcessorCreated(processorToDelete);
    Processor::setPluginWindowType(processorToDelete, pluginWindowType);
    disconnectProcessorAction.undo();

    return true;
}

bool DeleteProcessor::performTemporary() {
    disconnectProcessorAction.perform();
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.removeChild(processorToDelete, nullptr);
    else
        Track::getProcessorLane(track).removeChild(processorToDelete, nullptr);
    return true;
}

bool DeleteProcessor::undoTemporary() {
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        Track::getProcessorLane(track).addChild(processorToDelete, processorIndex, nullptr);
    disconnectProcessorAction.undo();
    return true;
}
