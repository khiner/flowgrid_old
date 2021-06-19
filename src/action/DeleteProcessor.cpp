#include "DeleteProcessor.h"

#include "view/PluginWindowType.h"

DeleteProcessor::DeleteProcessor(Processor *processor, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph)
        : trackIndex(tracks.getTrackForProcessor(processor)->getIndex()), processorSlot(processor->isTrackIOProcessor() ? -1 : processor->getSlot()), processorIndex(processor->getIndex()),
          pluginWindowType(processor->getPluginWindowType()), graphIsFrozen(processorGraph.areAudioGraphUpdatesPaused()), processorState(processorGraph.getProcessorWrappers().saveProcessorInformationToState(processor)),
          disconnectProcessorAction(DisconnectProcessor(connections, processor, all, true, true, true, true)),
          tracks(tracks), processorGraph(processorGraph) {}

bool DeleteProcessor::perform() {
    tracks.getProcessorAt(trackIndex, processorSlot)->setPluginWindowType(static_cast<int>(PluginWindowType::none));
    performTemporary(true);
    return true;
}

bool DeleteProcessor::undo() {
    undoTemporary(true);
    tracks.getProcessorAt(trackIndex, processorSlot)->setPluginWindowType(pluginWindowType);

    return true;
}

bool DeleteProcessor::performTemporary(bool apply) {
    if (!apply) processorGraph.pauseAudioGraphUpdates();
    disconnectProcessorAction.perform();
    if (processorSlot == -1) {
        tracks.getChild(trackIndex)->getState().removeChild(processorState, nullptr);
    } else {
        tracks.getChild(trackIndex)->getProcessorLane()->remove(processorIndex);
    }
    if (!graphIsFrozen && apply) processorGraph.resumeAudioGraphUpdatesAndApplyDiffSincePause();
    return true;
}

bool DeleteProcessor::undoTemporary(bool apply) {
    if (!apply) processorGraph.pauseAudioGraphUpdates();
    if (processorSlot == -1) {
        tracks.getChild(trackIndex)->getState().appendChild(processorState, nullptr);
    } else {
        tracks.getChild(trackIndex)->getProcessorLane()->add(processorState, processorIndex);
    }
    disconnectProcessorAction.undo();
    if (!graphIsFrozen && apply) processorGraph.resumeAudioGraphUpdatesAndApplyDiffSincePause();
    return true;
}
