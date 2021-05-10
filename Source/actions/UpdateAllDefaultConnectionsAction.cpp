#include "UpdateAllDefaultConnectionsAction.h"

UpdateAllDefaultConnectionsAction::UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections, TracksState &tracks, ConnectionsState &connections, InputState &input,
                                                                     OutputState &output, StatefulAudioProcessorContainer &audioProcessorContainer, ValueTree trackToTreatAsFocused)
        : CreateOrDeleteConnectionsAction(connections) {
    for (const auto &track : tracks.getState()) {
        for (const auto &processor : TracksState::getAllProcessorsForTrack(track))
            coalesceWith(UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, output, audioProcessorContainer));
    }

    if (resetDefaultExternalInputConnections) {
        perform();
        auto resetAction = ResetDefaultExternalInputConnectionsAction(connections, tracks, input, audioProcessorContainer, std::move(trackToTreatAsFocused));
        undo();
        coalesceWith(resetAction);
    }
}
