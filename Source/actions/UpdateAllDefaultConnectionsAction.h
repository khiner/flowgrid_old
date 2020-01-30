#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"
#include "UpdateProcessorDefaultConnectionsAction.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

#include <Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state_managers/InputStateManager.h>

struct UpdateAllDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    
    UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom,
                                      ConnectionsStateManager &connectionsManager, TracksStateManager &tracksManager,
                                      InputStateManager &inputManager, StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateOrDeleteConnectionsAction(connectionsManager) {
        for (const auto& track : tracksManager.getState()) {
            for (const auto& processor : track) {
                coalesceWith(UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connectionsManager, audioProcessorContainer));
            }
        }
        coalesceWith(ResetDefaultExternalInputConnectionsAction(true, connectionsManager, tracksManager, inputManager, audioProcessorContainer));
    }

private:
    JUCE_DECLARE_NON_COPYABLE(UpdateAllDefaultConnectionsAction)
};
