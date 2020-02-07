#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>
#include <actions/DeleteProcessorAction.h>

#include "JuceHeader.h"

struct DuplicateTrackAction : public UndoableAction {
    DuplicateTrackAction(const ValueTree& trackToDuplicate, TracksState &tracks, ConnectionsState &connections, ViewState &view,
                         StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : trackToDuplicate(trackToDuplicate), trackIndex(trackToDuplicate.getParent().indexOf(trackToDuplicate)),
              createTrackAction(false, trackToDuplicate, true, tracks, connections, view) {
            for (auto processor : trackToDuplicate) {
                duplicateProcessorActions.add(new DuplicateProcessorAction(processor, tracks.indexOf(trackToDuplicate) + 1, int(processor[IDs::processorSlot]) + 1,
                                                                           tracks, view, audioProcessorContainer, pluginManager));
            }
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
