#include "UpdateAllDefaultConnectionsAction.h"

UpdateAllDefaultConnectionsAction::UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections, TracksState &tracks, ConnectionsState &connections, InputState &input,
                                                                     OutputState &output, ProcessorGraph &processorGraph, ValueTree trackToTreatAsFocused)
        : CreateOrDeleteConnectionsAction(connections) {
    for (const auto &track : tracks.getState()) {
        for (const auto &processor : TracksState::getAllProcessorsForTrack(track))
            coalesceWith(UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, output, processorGraph));
    }

    if (resetDefaultExternalInputConnections) {
        perform();
        auto resetAction = ResetDefaultExternalInputConnectionsAction(connections, tracks, input, processorGraph, std::move(trackToTreatAsFocused));
        undo();
        coalesceWith(resetAction);
    }
}
