#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

// TODO should copy and insert the entire selected range, not one-at-a-time
struct DuplicateSelectedItemsAction : public UndoableAction {
    DuplicateSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                                 StatefulAudioProcessorContainer &audioProcessorContainer, PluginManager &pluginManager)
            : selectAction(createSelectAction(tracks, connections, view, input, audioProcessorContainer)) {
        for (auto &selectedItem : tracks.findAllSelectedItems()) {
            if (selectedItem.hasType(IDs::TRACK) && !TracksState::isMasterTrack(selectedItem)) {
                createTrackActions.add(new CreateTrackAction(false, selectedItem, true, tracks, connections, view));
                createTrackActions.getLast()->perform();
                for (auto processor : selectedItem) {
                    createProcessorActions.add(new CreateProcessorAction(createProcessor(processor, tracks),  *pluginManager.getDescriptionForIdentifier(processor[IDs::id]),
                                                                         tracks.indexOf(selectedItem) + 1, processor[IDs::processorSlot],
                                                                         tracks, view, audioProcessorContainer));
                    // Insert indexes will depend on how many processors are in the track at action creation time,
                    // so we actually need to perform as we go and undo all after.
                    createProcessorActions.getLast()->perform();
                }
                for (int i = createProcessorActions.size() - 1; i >= 0; i--)
                    createProcessorActions.getUnchecked(i)->undo();
                createTrackActions.getLast()->undo();
            } else if (selectedItem.hasType(IDs::PROCESSOR) && !TracksState::isMixerChannelProcessor(selectedItem)) {
                createProcessorActions.add(new CreateProcessorAction(createProcessor(selectedItem, tracks),
                                                                     *pluginManager.getDescriptionForIdentifier(selectedItem[IDs::id]),
                                                                     tracks.indexOf(selectedItem.getParent()),
                                                                     int(selectedItem[IDs::processorSlot]) + 1,
                                                                     tracks, view, audioProcessorContainer));
            }
        }
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
    OwnedArray<CreateTrackAction> createTrackActions;
    OwnedArray<CreateProcessorAction> createProcessorActions;
    SelectAction selectAction;

    static ValueTree createProcessor(ValueTree& fromProcessor, TracksState &tracks) {
        tracks.saveProcessorStateInformationToState(fromProcessor);
        auto duplicatedProcessor = fromProcessor.createCopy();
        duplicatedProcessor.removeProperty(IDs::nodeId, nullptr);
        return duplicatedProcessor;
    }

    static SelectAction createSelectAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                                           StatefulAudioProcessorContainer &audioProcessorContainer) {
        SelectAction selectAction = {tracks, connections, view, input, audioProcessorContainer};
        return selectAction;
    }

    JUCE_DECLARE_NON_COPYABLE(DuplicateSelectedItemsAction)
};
