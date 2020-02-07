#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"
#include "DuplicateProcessorAction.h"
#include "DuplicateTrackAction.h"

// TODO should copy and insert the entire selected range, not one-at-a-time
struct DuplicateSelectedItemsAction : public UndoableAction {
    DuplicateSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, ViewState &view,
                                 StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager) {
        for (auto &selectedItem : tracks.findAllSelectedItems()) {
            if (selectedItem.hasType(IDs::TRACK) && !TracksState::isMasterTrack(selectedItem))
                duplicateTrackActions.add(new DuplicateTrackAction(selectedItem, tracks, connections, view, audioProcessorContainer, pluginManager));
            else if (selectedItem.hasType(IDs::PROCESSOR) && !TracksState::isMixerChannelProcessor(selectedItem))
                duplicateProcessorActions.add(new DuplicateProcessorAction(selectedItem, tracks, view, audioProcessorContainer, pluginManager));
        }
    }

    bool perform() override {
        for (auto *duplicateProcessorAction : duplicateProcessorActions)
            duplicateProcessorAction->perform();
        for (auto *duplicateTrackAction : duplicateTrackActions)
            duplicateTrackAction->perform();

        return !duplicateProcessorActions.isEmpty() || !duplicateTrackActions.isEmpty();
    }

    bool undo() override {
        for (auto *duplicateTrackAction : duplicateTrackActions)
            duplicateTrackAction->undo();
        for (auto *duplicateProcessorAction : duplicateProcessorActions)
            duplicateProcessorAction->undo();

        return !duplicateProcessorActions.isEmpty() || !duplicateTrackActions.isEmpty();
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    OwnedArray<DuplicateTrackAction> duplicateTrackActions;
    OwnedArray<DuplicateProcessorAction> duplicateProcessorActions;

    JUCE_DECLARE_NON_COPYABLE(DuplicateSelectedItemsAction)
};
