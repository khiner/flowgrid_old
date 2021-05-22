#include "DeleteProcessor.h"

#include "view/PluginWindow.h"

DeleteProcessor::DeleteProcessor(const ValueTree &processorToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph)
        : tracks(tracks), trackIndex(tracks.indexOf(Track::getTrackForProcessor(processorToDelete))),
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
    auto track = tracks.getTrack(trackIndex);
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
    auto track = tracks.getTrack(trackIndex);
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.removeChild(processorToDelete, nullptr);
    else
        Track::getProcessorLane(track).removeChild(processorToDelete, nullptr);
    return true;
}

bool DeleteProcessor::undoTemporary() {
    auto track = tracks.getTrack(trackIndex);
    if (Processor::isTrackIOProcessor(processorToDelete))
        track.appendChild(processorToDelete, nullptr);
    else
        Track::getProcessorLane(track).addChild(processorToDelete, processorIndex, nullptr);
    disconnectProcessorAction.undo();
    return true;
}
