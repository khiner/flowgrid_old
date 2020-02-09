#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

// TODO should copy and insert the entire selected range, not one-at-a-time
struct DuplicateSelectedItemsAction : public UndoableAction {
    DuplicateSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                                 PluginManager &pluginManager, StatefulAudioProcessorContainer &audioProcessorContainer) {
        auto selectedTracks = tracks.findSelectedNonMasterTracks();
        auto nonSelectedTracks = tracks.findNonSelectedTracks();
        std::vector<int> selectedTrackIndices;
        for (const auto& selectedTrack : selectedTracks)
            selectedTrackIndices.push_back(tracks.indexOf(selectedTrack));

        auto duplicatedTrackIndices = findDuplicationIndices(selectedTrackIndices);
        int selectedTrackIndex = 0;
        for (const auto& selectedTrack : selectedTracks) {
            int duplicatedTrackIndex = duplicatedTrackIndices[selectedTrackIndex];
            createTrackActions.add(new CreateTrackAction(duplicatedTrackIndex, false, selectedTrack, tracks, connections, view));
            createTrackActions.getLast()->perform();
            for (const auto& processor : selectedTrack)
                addAndPerformCreateProcessorAction(processor, duplicatedTrackIndex, processor[IDs::processorSlot],
                                                   tracks, view, pluginManager, audioProcessorContainer);
            selectedTrackIndex++;
        }
        for (const auto& nonSelectedTrack : nonSelectedTracks) {
            auto selectedProcessors = TracksState::findSelectedProcessorsForTrack(nonSelectedTrack);
            selectedProcessors.removeIf([](const ValueTree& processor) { return TracksState::isMixerChannelProcessor(processor); });

            std::vector<int> selectedProcessorSlots;
            for (const auto& selectedProcessor : selectedProcessors)
                selectedProcessorSlots.push_back(selectedProcessor[IDs::processorSlot]);
            auto duplicatedProcessorSlots = findDuplicationIndices(selectedProcessorSlots);

            int selectedProcessorIndex = 0;
            for (const auto& selectedProcessor : selectedProcessors) {
                int duplicatedProcessorSlot = duplicatedProcessorSlots[selectedProcessorIndex];
                addAndPerformCreateProcessorAction(selectedProcessor, tracks.indexOf(nonSelectedTrack), duplicatedProcessorSlot,
                                                   tracks, view, pluginManager, audioProcessorContainer);
                selectedProcessorIndex++;
            }
        }

        selectAction = std::make_unique<MoveSelectionsAction>(createTrackActions, createProcessorActions, tracks, connections, view, input, audioProcessorContainer);
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

        selectAction->perform();

        return !createTrackActions.isEmpty() || !createProcessorActions.isEmpty();
    }

    bool undo() override {
        selectAction->undo();

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
        MoveSelectionsAction(const OwnedArray<CreateTrackAction>& createTrackActions,
                             const OwnedArray<CreateProcessorAction>& createProcessorActions,
                             TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
                : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
            for (int i = 0; i < newTrackSelections.size(); i++) {
                newTrackSelections.setUnchecked(i, false);
                newSelectedSlotsMasks.setUnchecked(i, BigInteger().toString(2));
            }
            for (auto* createProcessorAction : createProcessorActions) {
                String maskString = newSelectedSlotsMasks.getUnchecked(createProcessorAction->trackIndex);
                BigInteger mask;
                mask.parseString(maskString, 2);
                mask.setBit(createProcessorAction->slot, true);
                newSelectedSlotsMasks.setUnchecked(createProcessorAction->trackIndex, mask.toString(2));
            }
            for (auto* createTrackAction : createTrackActions) {
                newTrackSelections.setUnchecked(createTrackAction->insertIndex, true);
                const auto& track = tracks.getTrack(createTrackAction->insertIndex);
                newSelectedSlotsMasks.setUnchecked(createTrackAction->insertIndex, tracks.createFullSelectionBitmask(track));
            }
        }
    };

    OwnedArray<CreateTrackAction> createTrackActions;
    OwnedArray<CreateProcessorAction> createProcessorActions;
    std::unique_ptr<SelectAction> selectAction;

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

    static std::vector<int> findDuplicationIndices(std::vector<int> currentIndices) {
        auto duplicationIndices = currentIndices;
        int previousIndex = -1;
        int endOfContiguousRange = 0;
        for (int i = 0; i < duplicationIndices.size(); i++) {
            int currentIndex = currentIndices[i];
            if (previousIndex != -1 && currentIndex - previousIndex > 1)
                endOfContiguousRange = i;
            for (int j = endOfContiguousRange; j < duplicationIndices.size(); j++)
                duplicationIndices[j] += 1;
            previousIndex = currentIndex;
        }

        return duplicationIndices;
    }

    JUCE_DECLARE_NON_COPYABLE(DuplicateSelectedItemsAction)
};
