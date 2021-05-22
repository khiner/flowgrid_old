#include "UpdateAllDefaultConnections.h"

UpdateAllDefaultConnections::UpdateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections, Tracks &tracks, Connections &connections, Input &input,
                                                         Output &output, ProcessorGraph &processorGraph, ValueTree trackToTreatAsFocused)
        : CreateOrDeleteConnections(connections) {
    for (const auto &track : tracks.getState()) {
        for (const auto &processor : Track::getAllProcessors(track))
            coalesceWith(UpdateProcessorDefaultConnections(processor, makeInvalidDefaultsIntoCustom, connections, output, processorGraph));
    }

    if (resetDefaultExternalInputConnections) {
        perform();
        auto resetAction = ResetDefaultExternalInputConnectionsAction(connections, tracks, input, processorGraph, std::move(trackToTreatAsFocused));
        undo();
        coalesceWith(resetAction);
    }
}
