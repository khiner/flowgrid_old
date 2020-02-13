#pragma once

#include "JuceHeader.h"

// Inserting a processor "pushes" any contiguous set of processors starting at the given slot down.
// (The new processor always "wins" by keeping its given slot.)
// Doesn't take care of any select actions! (Caller is responsible for that.)
struct InsertProcessorAction : UndoableAction {
    InsertProcessorAction(const ValueTree &processor, int toTrackIndex, int toSlot, TracksState &tracks, ViewState &view)
            : addOrMoveProcessorAction(processor, toTrackIndex, toSlot, tracks, view),
              makeSlotsValidAction(createMakeSlotsValidAction(toTrackIndex, tracks, view)) {
        // cleanup - yeah it's ugly but avoids need for some copy/move madness in createMakeSlotsValidAction
        addOrMoveProcessorAction.undo();
    }

    bool perform() override {
        addOrMoveProcessorAction.perform();
        makeSlotsValidAction.perform();
        return true;
    }

    bool undo() override {
        makeSlotsValidAction.undo();
        addOrMoveProcessorAction.undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:

    struct SetProcessorSlotAction : public UndoableAction {
        SetProcessorSlotAction(int trackIndex, const ValueTree& processor, int newSlot,
                               TracksState &tracks, ViewState &view)
                : processor(processor), oldSlot(processor[IDs::processorSlot]), newSlot(newSlot) {
            const auto& track = tracks.getTrack(trackIndex);
            const auto& mixerChannel = tracks.getMixerChannelProcessorForTrack(track);
            if ((mixerChannel.isValid() && mixerChannel != this->processor) ||
                (!mixerChannel.isValid() && !TracksState::isMixerChannelProcessor(this->processor))) {
                for (int i = 0; i <= this->newSlot - tracks.getMixerChannelSlotForTrack(track); i++)
                    addProcessorRowActions.add(new AddProcessorRowAction(trackIndex, tracks, view));
            }
        }

        bool perform() override {
            for (auto *addProcessorRowAction : addProcessorRowActions)
                addProcessorRowAction->perform();
            if (processor.isValid())
                processor.setProperty(IDs::processorSlot, newSlot, nullptr);
            return true;
        }

        bool undo() override {
            if (processor.isValid())
                processor.setProperty(IDs::processorSlot, oldSlot, nullptr);
            for (auto *addProcessorRowAction : addProcessorRowActions)
                addProcessorRowAction->undo();
            return true;
        }

    private:
        ValueTree processor;
        int oldSlot, newSlot;

        struct AddProcessorRowAction : public UndoableAction {
            AddProcessorRowAction(int trackIndex, TracksState &tracks, ViewState &view)
                    : trackIndex(trackIndex), tracks(tracks), view(view) {}

            bool perform() override {
                const auto& track = tracks.getTrack(trackIndex);
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

                for (auto& processorToIncrement : processorsNeedingSlotIncrement)
                    if (processorToIncrement.isValid())
                        processorToIncrement.setProperty(IDs::processorSlot, int(processorToIncrement[IDs::processorSlot]) + 1, nullptr);

                return true;
            }

            bool undo() override {
                const auto& track = tracks.getTrack(trackIndex);
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
                for (auto& processorToDecrement : processorsNeedingSlotDecrement) {
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
        AddOrMoveProcessorAction(const ValueTree& processor, int newTrackIndex, int newSlot, TracksState &tracks, ViewState &view)
                : processor(processor), oldTrackIndex(tracks.indexOf(processor.getParent())), newTrackIndex(newTrackIndex),
                  oldSlot(processor[IDs::processorSlot]), newSlot(newSlot),
                  oldIndex(tracks.getTrack(oldTrackIndex).indexOf(processor)),
                  newIndex(tracks.getInsertIndexForProcessor(tracks.getTrack(newTrackIndex), processor, this->newSlot)),
                  setProcessorSlotAction(newTrackIndex, processor, newSlot, tracks, view),
                  tracks(tracks) {}

        bool perform() override {
            if (processor.isValid()) {
                const ValueTree &oldTrack = tracks.getTrack(oldTrackIndex);
                ValueTree newTrack = tracks.getTrack(newTrackIndex);

                if (!oldTrack.isValid()) // only inserting, not moving from another track
                    newTrack.addChild(processor, newIndex, nullptr);
                else if (oldTrack == newTrack)
                    newTrack.moveChild(oldIndex, newIndex, nullptr);
                else
                    newTrack.moveChildFromParent(oldTrack, oldIndex, newIndex, nullptr);
            }
            setProcessorSlotAction.perform();

            return true;
        }

        bool undo() override {
            setProcessorSlotAction.undo();

            if (processor.isValid()) {
                ValueTree oldTrack = tracks.getTrack(oldTrackIndex);
                ValueTree newTrack = tracks.getTrack(newTrackIndex);

                if (!oldTrack.isValid()) // only inserting, not moving from another track
                    newTrack.removeChild(processor, nullptr);
                else if (oldTrack == newTrack)
                    newTrack.moveChild(newIndex, oldIndex, nullptr);
                else
                    oldTrack.moveChildFromParent(newTrack, newIndex, oldIndex, nullptr);
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

    struct MakeSlotsValidAction : public UndoableAction {
        MakeSlotsValidAction(int trackIndex, TracksState &tracks, ViewState &view) : tracks(tracks) {
            const ValueTree& track = tracks.getTrack(trackIndex);
            std::vector<int> slots;
            for (const ValueTree& child : track)
                slots.push_back(child[IDs::processorSlot]);
            sort(slots.begin(), slots.end());
            for (int i = 1; i < slots.size(); i++)
                while (slots[i] <= slots[i - 1])
                    slots[i] += 1;

            auto iterator = slots.begin();
            for (const ValueTree& processor : track) {
                int newSlot = *(iterator++);
                // Need to actually _do_ the move for each processor, since this could affect the results of
                // a later processor slot move. (This action is undone later.)
                setSlotActions.add(new SetProcessorSlotAction(trackIndex, processor, newSlot, tracks, view));
                setSlotActions.getLast()->perform();
            }
            for (int i = setSlotActions.size() - 1; i >= 0; i--)
                setSlotActions.getUnchecked(i)->undo();
        }

        bool perform() override {
            for (auto* setSlotAction : setSlotActions)
                setSlotAction->perform();
            return true;
        }

        bool undo() override {
            for (int i = setSlotActions.size() - 1; i >= 0; i--)
                setSlotActions.getUnchecked(i)->undo();
            return true;
        }

        int getSizeInUnits() override {
            return (int)sizeof(*this); //xxx should be more accurate
        }

    private:
        TracksState &tracks;

        OwnedArray<SetProcessorSlotAction> setSlotActions;

        JUCE_DECLARE_NON_COPYABLE(MakeSlotsValidAction)
    };

    MakeSlotsValidAction createMakeSlotsValidAction(int trackIndex, TracksState &tracks, ViewState &view) {
        addOrMoveProcessorAction.perform();
        return { trackIndex, tracks, view };
    }

    AddOrMoveProcessorAction addOrMoveProcessorAction;
    MakeSlotsValidAction makeSlotsValidAction;
};
