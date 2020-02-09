#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"

// TODO update focus
struct DuplicateSelectedItemsAction : public UndoableAction {
    DuplicateSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, ViewState &view,
                                 InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer) {
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
                                                   tracks, view, audioProcessorContainer);
            selectedTrackIndex++;
        }
        for (const auto& nonSelectedTrack : nonSelectedTracks) {
            const BigInteger slotsMask = TracksState::getSlotMask(nonSelectedTrack);

            std::vector<int> selectedSlots;
            for (int slot = 0; slot <= std::min(tracks.getMixerChannelSlotForTrack(nonSelectedTrack) - 1, slotsMask.getHighestBit()); slot++)
                if (slotsMask[slot])
                    selectedSlots.push_back(slot);

            auto duplicatedSlots = findDuplicationIndices(selectedSlots);

            for (int i = 0; i < selectedSlots.size(); i++) {
                const auto& processor = TracksState::getProcessorAtSlot(nonSelectedTrack, selectedSlots[i]);
                addAndPerformCreateProcessorAction(processor, tracks.indexOf(nonSelectedTrack), duplicatedSlots[i],
                                                   tracks, view, audioProcessorContainer);
            }
        }

        selectAction = std::make_unique<MoveSelectionsAction>(createTrackActions, createProcessorActions, tracks, connections, view, input, audioProcessorContainer);
        for (int i = createProcessorActions.size() - 1; i >= 0; i--)
            createProcessorActions.getUnchecked(i)->undoTemporary();
        for (int i = createTrackActions.size() - 1; i >= 0; i--)
            createTrackActions.getUnchecked(i)->undo();
    }

    bool perform() override {
        if (createTrackActions.isEmpty() && createProcessorActions.isEmpty())
            return false;

        for (auto *createTrackAction : createTrackActions)
            createTrackAction->perform();
        for (auto *createProcessorAction : createProcessorActions)
            createProcessorAction->perform();
        selectAction->perform();

        return true;
    }

    bool undo() override {
        if (createTrackActions.isEmpty() && createProcessorActions.isEmpty())
            return false;

        selectAction->undo();

        for (int i = createProcessorActions.size() - 1; i >= 0; i--)
            createProcessorActions.getUnchecked(i)->undo();
        for (int i = createTrackActions.size() - 1; i >= 0; i--)
            createTrackActions.getUnchecked(i)->undo();

        return true;
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
                                            TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer) {
        createProcessorActions.add(new CreateProcessorAction(createProcessor(processor, tracks), trackIndex, slot, tracks, view, audioProcessorContainer));
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
