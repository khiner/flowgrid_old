#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"
#include "UpdateProcessorDefaultConnectionsAction.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

#include <state/Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state/InputState.h>

#include <utility>

struct UpdateAllDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    
    UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections,
                                      ConnectionsState &connections, TracksState &tracks, InputState &input,
                                      StatefulAudioProcessorContainer &audioProcessorContainer,
                                      ValueTree trackToTreatAsFocused={})
            : CreateOrDeleteConnectionsAction(connections) {
        for (const auto& track : tracks.getState())
            for (const auto& processor : track)
                coalesceWith(UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, audioProcessorContainer));

        if (resetDefaultExternalInputConnections) {
            perform();
            auto resetAction = ResetDefaultExternalInputConnectionsAction(true, connections, tracks, input, audioProcessorContainer, std::move(trackToTreatAsFocused));
            undo();
            coalesceWith(resetAction);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE(UpdateAllDefaultConnectionsAction)
};
