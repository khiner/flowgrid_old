#pragma once

#include "JuceHeader.h"

// Inserting a processor "pushes" any contiguous set of processors starting at the given slot down.
// (The new processor always "wins" by keeping its given slot.)
// Doesn't take care of any select actions! (Caller is responsible for that.)
struct InsertProcessorAction : UndoableAction {
    InsertProcessorAction(const ValueTree &processor, int toTrackIndex, int toSlot, TracksState &tracks, ViewState &view)
            : addOrMoveProcessorAction(processor, toTrackIndex, toSlot, tracks, view) {}

    bool perform() override {
        addOrMoveProcessorAction.perform();
        return true;
    }

    bool undo() override {
        addOrMoveProcessorAction.undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:

    struct SetProcessorSlotAction : public UndoableAction {
        SetProcessorSlotAction(int trackIndex, const ValueTree &processor, int newSlot,
                               TracksState &tracks, ViewState &view)
                : processor(processor), oldSlot(processor[IDs::processorSlot]), newSlot(newSlot) {
            const auto &track = tracks.getTrack(trackIndex);
            const auto &mixerChannel = tracks.getMixerChannelProcessorForTrack(track);
            if ((mixerChannel.isValid() && mixerChannel != this->processor) ||
                (!mixerChannel.isValid() && !TracksState::isMixerChannelProcessor(this->processor))) {
                for (int i = 0; i <= this->newSlot - tracks.getMixerChannelSlotForTrack(track); i++)
                    addProcessorRowActions.add(new AddProcessorRowAction(trackIndex, tracks, view));
            }
            if (addProcessorRowActions.isEmpty()) {
                const auto &conflictingProcessor = tracks.getProcessorAtSlot(track, newSlot);
                if (conflictingProcessor.isValid())
                    pushConflictingProcessorAction = std::make_unique<SetProcessorSlotAction>(trackIndex, conflictingProcessor, newSlot + 1, tracks, view);
            }
        }

        bool perform() override {
            for (auto *addProcessorRowAction : addProcessorRowActions)
                addProcessorRowAction->perform();
            if (pushConflictingProcessorAction)
                pushConflictingProcessorAction->perform();
            if (processor.isValid())
                processor.setProperty(IDs::processorSlot, newSlot, nullptr);
            return true;
        }

        bool undo() override {
            if (processor.isValid())
                processor.setProperty(IDs::processorSlot, oldSlot, nullptr);
            if (pushConflictingProcessorAction)
                pushConflictingProcessorAction->undo();
            for (auto *addProcessorRowAction : addProcessorRowActions)
                addProcessorRowAction->undo();
            return true;
        }

    private:
        ValueTree processor;
        int oldSlot, newSlot;
        std::unique_ptr<SetProcessorSlotAction> pushConflictingProcessorAction;

        struct AddProcessorRowAction : public UndoableAction {
            AddProcessorRowAction(int trackIndex, TracksState &tracks, ViewState &view)
                    : trackIndex(trackIndex), tracks(tracks), view(view) {}

            bool perform() override {
                const auto &track = tracks.getTrack(trackIndex);
                Array<ValueTree> processorsNeedingSlotIncrement;
                if (TracksState::isMasterTrack(track)) {
                    processorsNeedingSlotIncrement.add(tracks.getMixerChannelProcessorForTrack(track));
                    view.getState().setProperty(IDs::numMasterProcessorSlots, view.getNumMasterProcessorSlots() + 1, nullptr);
                } else {
                    for (const auto &nonMasterTrack : tracks.getState()) {
                        if (!TracksState::isMasterTrack(nonMasterTrack))
                            processorsNeedingSlotIncrement.add(tracks.getMixerChannelProcessorForTrack(nonMasterTrack));
                    }
                    view.getState().setProperty(IDs::numProcessorSlots, view.getNumTrackProcessorSlots() + 1, nullptr);
                }

                for (auto &processorToIncrement : processorsNeedingSlotIncrement)
                    if (processorToIncrement.isValid())
                        processorToIncrement.setProperty(IDs::processorSlot, int(processorToIncrement[IDs::processorSlot]) + 1, nullptr);

                return true;
            }

            bool undo() override {
                const auto &track = tracks.getTrack(trackIndex);
                Array<ValueTree> processorsNeedingSlotDecrement;
                if (TracksState::isMasterTrack(track)) {
                    processorsNeedingSlotDecrement.add(tracks.getMixerChannelProcessorForTrack(track));
                    view.getState().setProperty(IDs::numMasterProcessorSlots, view.getNumMasterProcessorSlots() - 1, nullptr);
                } else {
                    for (const auto &nonMasterTrack : tracks.getState()) {
                        if (!TracksState::isMasterTrack(nonMasterTrack))
                            processorsNeedingSlotDecrement.add(tracks.getMixerChannelProcessorForTrack(nonMasterTrack));
                    }
                    view.getState().setProperty(IDs::numProcessorSlots, view.getNumTrackProcessorSlots() - 1, nullptr);
                }
                for (auto &processorToDecrement : processorsNeedingSlotDecrement) {
                    if (processorToDecrement.isValid())
                        processorToDecrement.setProperty(IDs::processorSlot, int(processorToDecrement[IDs::processorSlot]) - 1, nullptr);
                }

                return true;
            }

        private:
            int trackIndex;

            TracksState &tracks;
            ViewState &view;
        };

        OwnedArray<AddProcessorRowAction> addProcessorRowActions;
    };

    struct AddOrMoveProcessorAction : public UndoableAction {
        AddOrMoveProcessorAction(const ValueTree &processor, int newTrackIndex, int newSlot, TracksState &tracks, ViewState &view)
                : processor(processor), oldTrackIndex(tracks.indexOf(TracksState::getTrackForProcessor(processor))), newTrackIndex(newTrackIndex),
                  oldSlot(processor[IDs::processorSlot]), newSlot(newSlot),
                  oldIndex(processor.getParent().indexOf(processor)),
                  newIndex(tracks.getInsertIndexForProcessor(tracks.getTrack(newTrackIndex), processor, this->newSlot)),
                  setProcessorSlotAction(newTrackIndex, processor, newSlot, tracks, view),
                  tracks(tracks) {}

        bool perform() override {
            if (processor.isValid()) {
                const ValueTree &oldTrack = tracks.getTrack(oldTrackIndex);
                const ValueTree newTrack = tracks.getTrack(newTrackIndex);
                const auto oldLane = TracksState::getProcessorLaneForTrack(oldTrack);
                auto newLane = TracksState::getProcessorLaneForTrack(newTrack);

                if (!oldLane.isValid()) // only inserting, not moving from another track
                    newLane.addChild(processor, newIndex, nullptr);
                else if (oldLane == newTrack)
                    newLane.moveChild(oldIndex, newIndex, nullptr);
                else
                    newLane.moveChildFromParent(oldLane, oldIndex, newIndex, nullptr);
            }
            setProcessorSlotAction.perform();

            return true;
        }

        bool undo() override {
            setProcessorSlotAction.undo();

            if (processor.isValid()) {
                const auto &oldTrack = tracks.getTrack(oldTrackIndex);
                const auto &newTrack = tracks.getTrack(newTrackIndex);
                auto oldLane = TracksState::getProcessorLaneForTrack(oldTrack);
                auto newLane = TracksState::getProcessorLaneForTrack(newTrack);

                if (!oldTrack.isValid()) // only inserting, not moving from another track
                    newLane.removeChild(processor, nullptr);
                else if (oldTrack == newTrack)
                    newLane.moveChild(newIndex, oldIndex, nullptr);
                else
                    oldLane.moveChildFromParent(newLane, newIndex, oldIndex, nullptr);
            }

            return true;
        }

    private:
        ValueTree processor;
        int oldTrackIndex, newTrackIndex;
        int oldSlot, newSlot;
        int oldIndex, newIndex;

        SetProcessorSlotAction setProcessorSlotAction;

        TracksState &tracks;
    };

    AddOrMoveProcessorAction addOrMoveProcessorAction;
};
