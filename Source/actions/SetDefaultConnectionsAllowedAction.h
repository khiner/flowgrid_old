#pragma once

#include <state/Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include "JuceHeader.h"
#include "CreateOrDeleteConnectionsAction.h"
#include "DisconnectProcessorAction.h"

struct SetDefaultConnectionsAllowedAction : public CreateOrDeleteConnectionsAction {
    SetDefaultConnectionsAllowedAction(const ValueTree& processor, bool defaultConnectionsAllowed, ConnectionsState& connections)
            : CreateOrDeleteConnectionsAction(connections), processor(processor), defaultConnectionsAllowed(defaultConnectionsAllowed) {
        if (!defaultConnectionsAllowed) {
            coalesceWith(DisconnectProcessorAction(connections, processor, all, true, false, true, true, AudioProcessorGraph::NodeID()));
        }
    }

    bool perform() override {
        processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, nullptr);
        CreateOrDeleteConnectionsAction::perform();
        return true;
    }

    bool undo() override {
        CreateOrDeleteConnectionsAction::perform();
        processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, nullptr);

        return true;
    }

private:
    ValueTree processor;
    bool defaultConnectionsAllowed;

    JUCE_DECLARE_NON_COPYABLE(SetDefaultConnectionsAllowedAction)
};
