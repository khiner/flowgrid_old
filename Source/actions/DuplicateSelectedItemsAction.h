#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

// TODO should copy and insert the entire selected range, not one-at-a-time
struct DuplicateSelectedItemsAction : public UndoableAction {
    DuplicateSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                                 StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : selectAction(tracks, connections, view, input, audioProcessorContainer) {
        const auto duplicatedTrackIndices = findDuplicatedTrackIndices(tracks);
        int selectedTrackIndex = 0;
        for (const auto& selectedItem : tracks.findAllSelectedItems()) {
            if (selectedItem.hasType(IDs::TRACK) && !TracksState::isMasterTrack(selectedItem)) {
                int duplicatedTrackIndex = duplicatedTrackIndices[selectedTrackIndex];
                createTrackActions.add(new CreateTrackAction(duplicatedTrackIndex, false, selectedItem, tracks, connections, view));
                createTrackActions.getLast()->perform();
                for (const auto& processor : selectedItem)
                    addAndPerformCreateProcessorAction(processor, duplicatedTrackIndex, processor[IDs::processorSlot],
                                                       tracks, view, pluginManager, audioProcessorContainer);
                selectedTrackIndex++;
            } else if (selectedItem.hasType(IDs::PROCESSOR) && !TracksState::isMixerChannelProcessor(selectedItem)) {
                // TODO findNewProcessorIndices
                addAndPerformCreateProcessorAction(selectedItem, tracks.indexOf(selectedItem.getParent()), int(selectedItem[IDs::processorSlot]) + 1,
                                                   tracks, view, pluginManager, audioProcessorContainer);
            }
        }
        for (int i = createProcessorActions.size() - 1; i >= 0; i--)
            createProcessorActions.getUnchecked(i)->undoTemporary();
        for (int i = createTrackActions.size() - 1; i >= 0; i--)
            createTrackActions.getUnchecked(i)->undo();
    }

    bool perform() override {
        for (auto* createTrackAction : createTrackActions)
            createTrackAction->perform();
        for (auto* createProcessorAction : createProcessorActions)
            createProcessorAction->perform();

//        selectAction.perform();

        return !createTrackActions.isEmpty() || !createProcessorActions.isEmpty();
    }

    bool undo() override {
//        selectAction.undo();

        for (int i = createProcessorActions.size() - 1; i >= 0; i--)
            createProcessorActions.getUnchecked(i)->undo();
        for (int i = createTrackActions.size() - 1; i >= 0; i--)
            createTrackActions.getUnchecked(i)->undo();

        return !createTrackActions.isEmpty() || !createProcessorActions.isEmpty();
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
                : SelectAction(tracks, connections, view, input, audioProcessorContainer) {}
    };

    OwnedArray<CreateTrackAction> createTrackActions;
    OwnedArray<CreateProcessorAction> createProcessorActions;
    MoveSelectionsAction selectAction;
    
    void addAndPerformCreateProcessorAction(ValueTree processor, int trackIndex, int slot,
                                            TracksState &tracks, ViewState &view, PluginManager &pluginManager,
                                            StatefulAudioProcessorContainer &audioProcessorContainer) {
        createProcessorActions.add(new CreateProcessorAction(createProcessor(processor, tracks),  *pluginManager.getDescriptionForIdentifier(processor[IDs::id]),
                                                             trackIndex, slot, tracks, view, audioProcessorContainer));
        // Insert indexes will depend on how many processors are in the track at action creation time,
        // so we actually need to perform as we go and undo all after.
        createProcessorActions.getLast()->performTemporary();
    }

    static ValueTree createProcessor(ValueTree& fromProcessor, TracksState &tracks) {
        tracks.saveProcessorStateInformationToState(fromProcessor);
        auto duplicatedProcessor = fromProcessor.createCopy();
        duplicatedProcessor.removeProperty(IDs::nodeId, nullptr);
        return duplicatedProcessor;
    }

    static std::vector<int> findDuplicatedTrackIndices(TracksState &tracks) {
        std::vector<int> oldTrackIndices;
        for (const auto& track : tracks.getState())
            if (track[IDs::selected] && !TracksState::isMasterTrack(track))
                oldTrackIndices.push_back(tracks.indexOf(track));

        auto newTrackIndices = oldTrackIndices;
        int previousIndex = -1;
        int endOfContiguousRange = 0;
        for (int i = 0; i < newTrackIndices.size(); i++) {
            int oldTrackIndex = oldTrackIndices[i];
            if (previousIndex != -1 && oldTrackIndex - previousIndex > 1)
                endOfContiguousRange = i;
            for (int j = endOfContiguousRange; j < newTrackIndices.size(); j++)
                newTrackIndices[j] += 1;
            previousIndex = oldTrackIndex;
        }

        return newTrackIndices;
    }

    JUCE_DECLARE_NON_COPYABLE(DuplicateSelectedItemsAction)
};
