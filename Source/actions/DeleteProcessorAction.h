#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"
#include "DisconnectProcessorAction.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree &processorToDelete, TracksState &tracks, ConnectionsState &connections,
                          StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : parentTrack(processorToDelete.getParent()), processorToDelete(processorToDelete),
              processorIndex(parentTrack.indexOf(processorToDelete)),
              disconnectProcessorAction(DisconnectProcessorAction(connections, processorToDelete, all, true, true, true, true)),
              audioProcessorContainer(audioProcessorContainer), pluginManager(pluginManager) {}

    bool perform() override {
        disconnectProcessorAction.perform();
        parentTrack.removeChild(processorToDelete, nullptr);
        pluginManager.closeWindowFor(StatefulAudioProcessorContainer::getNodeIdForState(processorToDelete));
        audioProcessorContainer.onProcessorDestroyed(processorToDelete);
        return true;
    }

    bool undo() override {
        parentTrack.addChild(processorToDelete, processorIndex, nullptr);
        audioProcessorContainer.onProcessorCreated(processorToDelete);
        disconnectProcessorAction.undo();

        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:
    ValueTree parentTrack;
    ValueTree processorToDelete;
    int processorIndex;
    DisconnectProcessorAction disconnectProcessorAction;

    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    JUCE_DECLARE_NON_COPYABLE(DeleteProcessorAction)
};
