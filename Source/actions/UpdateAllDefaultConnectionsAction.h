#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"
#include "UpdateProcessorDefaultConnectionsAction.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

#include <state/Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state/InputState.h>

struct UpdateAllDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    
    UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom,
                                      ConnectionsState &connections, TracksState &tracks, InputState &input,
                                      StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateOrDeleteConnectionsAction(connections) {
        for (const auto& track : tracks.getState()) {
            for (const auto& processor : track) {
                coalesceWith(UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, audioProcessorContainer));
            }
        }
        coalesceWith(ResetDefaultExternalInputConnectionsAction(true, connections, tracks, input, audioProcessorContainer));
    }

private:
    JUCE_DECLARE_NON_COPYABLE(UpdateAllDefaultConnectionsAction)
};
