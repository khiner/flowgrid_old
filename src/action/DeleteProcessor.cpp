#include "DeleteProcessor.h"

#include "view/PluginWindowType.h"

DeleteProcessor::DeleteProcessor(Processor *processor, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph)
        : trackIndex(tracks.getTrackForProcessor(processor)->getIndex()), processorSlot(processor->isTrackIOProcessor() ? -1 : processor->getSlot()), processorIndex(processor->getIndex()),
          pluginWindowType(processor->getPluginWindowType()), graphIsFrozen(processorGraph.areAudioGraphUpdatesPaused()), processorState(processorGraph.getProcessorWrappers().saveProcessorInformationToState(processor)),
          disconnectProcessorAction(DisconnectProcessor(connections, processor, all, true, true, true, true)),
          tracks(tracks), processorGraph(processorGraph) {}

bool DeleteProcessor::perform() {
    performTemporary(true);
    return true;
}

bool DeleteProcessor::undo() {
    undoTemporary(true);
    tracks.getProcessorAt(trackIndex, processorSlot)->setPluginWindowType(pluginWindowType);

    return true;
}

bool DeleteProcessor::performTemporary(bool apply) {
    disconnectProcessorAction.perform();
    auto *processor = tracks.getProcessorAt(trackIndex, processorSlot);
    if (apply) {
        processor->setPluginWindowType(static_cast<int>(PluginWindowType::none));
        processorGraph.onProcessorDestroyed(processor);
    }
    auto *track = tracks.get(trackIndex);
    if (processorSlot == -1) {
        track->getState().removeChild(processorState, nullptr);
    } else {
        track->getProcessorLane()->remove(processorIndex);
    }
    return true;
}

bool DeleteProcessor::undoTemporary(bool apply) {
    auto *track = tracks.get(trackIndex);
    if (processorSlot == -1) {
        track->getState().appendChild(processorState, nullptr);
    } else {
        track->getProcessorLane()->add(processorState, processorIndex);
    }
    auto *processor = tracks.getProcessorAt(trackIndex, processorSlot);
    if (apply) {
        processorGraph.onProcessorCreated(processor);
        processor->setPluginWindowType(pluginWindowType);
    }

    disconnectProcessorAction.undo();
    return true;
}
