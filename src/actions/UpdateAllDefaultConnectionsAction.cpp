#include "UpdateAllDefaultConnectionsAction.h"

UpdateAllDefaultConnectionsAction::UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections, Tracks &tracks, Connections &connections, Input &input,
                                                                     Output &output, ProcessorGraph &processorGraph, ValueTree trackToTreatAsFocused)
        : CreateOrDeleteConnectionsAction(connections) {
    for (const auto &track : tracks.getState()) {
        for (const auto &processor : Tracks::getAllProcessorsForTrack(track))
            coalesceWith(UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, output, processorGraph));
    }

    if (resetDefaultExternalInputConnections) {
        perform();
        auto resetAction = ResetDefaultExternalInputConnectionsAction(connections, tracks, input, processorGraph, std::move(trackToTreatAsFocused));
        undo();
        coalesceWith(resetAction);
    }
}
