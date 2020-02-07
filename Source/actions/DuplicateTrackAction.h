#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

struct DuplicateTrackAction : public UndoableAction {
    DuplicateTrackAction(const ValueTree& trackToDuplicate, TracksState &tracks, ConnectionsState &connections, ViewState &view,
                         StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : trackToDuplicate(trackToDuplicate), trackIndex(trackToDuplicate.getParent().indexOf(trackToDuplicate)),
              createTrackAction(false, trackToDuplicate, true, tracks, connections, view) {
            for (auto processor : trackToDuplicate) {
                duplicateProcessorActions.add(new DuplicateProcessorAction(processor, tracks.indexOf(trackToDuplicate) + 1, processor[IDs::processorSlot],
                                                                           tracks, view, audioProcessorContainer, pluginManager));
                // Insert indexes will depend on how many processors are in the track at action creation time,
                // so we actually need to perform as we go and undo all after.
                duplicateProcessorActions.getLast()->perform();
            }
        for (auto* duplicateProcessorAction : duplicateProcessorActions)
            duplicateProcessorAction->undo();
    }

    bool perform() override {
        createTrackAction.perform();
        for (auto* duplicateProcessorAction : duplicateProcessorActions)
            duplicateProcessorAction->perform();
        return true;
    }

    bool undo() override {
        for (auto* duplicateProcessorAction : duplicateProcessorActions)
            duplicateProcessorAction->undo();
        createTrackAction.undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    ValueTree trackToDuplicate;
    int trackIndex;

    CreateTrackAction createTrackAction;
    OwnedArray<DuplicateProcessorAction> duplicateProcessorActions;

    JUCE_DECLARE_NON_COPYABLE(DuplicateTrackAction)
};
