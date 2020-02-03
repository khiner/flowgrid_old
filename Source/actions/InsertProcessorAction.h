#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

// Inserting a processor "pushes" any contiguous set of processors starting at the given slot down.
// (The new processor always "wins" by keeping its given slot.)
// Doesn't take care of any select actions! (Caller is responsible for that.)
struct InsertProcessorAction : UndoableAction {
    InsertProcessorAction(ValueTree &toTrack, const ValueTree& processor, int toSlot,
                          TracksState &tracks, ViewState &view)
            : addOrMoveProcessorAction(processor, toTrack, toSlot, tracks),
              makeSlotsValidAction(createMakeSlotsValidAction(toTrack, tracks, view)) {
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

    struct AddOrMoveProcessorAction : public UndoableAction {
        AddOrMoveProcessorAction(const ValueTree& processor, ValueTree &toTrack, int newSlot, TracksState &tracks)
                : processor(processor), oldTrack(processor.getParent()), newTrack(toTrack),
                  oldIndex(oldTrack.indexOf(processor)), newIndex(tracks.getInsertIndexForSlot(newTrack, newSlot)),
                  oldSlot(processor[IDs::processorSlot]), newSlot(newSlot) {}

        bool perform() override {
            processor.setProperty(IDs::processorSlot, newSlot, nullptr);
            if (!oldTrack.isValid()) { // only inserting, not moving from another track
                newTrack.addChild(processor, newIndex, nullptr);
            } else {
                newTrack.moveChildFromParent(oldTrack, oldIndex, newIndex, nullptr);
            }

            return true;
        }

        bool undo() override {
            processor.setProperty(IDs::processorSlot, oldSlot, nullptr);
            if (!oldTrack.isValid()) { // only inserting, not moving from another track
                newTrack.removeChild(processor, nullptr);
            } else {
                oldTrack.moveChildFromParent(newTrack, newIndex, oldIndex, nullptr);
            }

            return true;
        }

    private:
        ValueTree processor;
        ValueTree oldTrack, newTrack;
        int oldIndex, newIndex;
        int oldSlot, newSlot;
    };

    struct MakeSlotsValidAction : public UndoableAction {
        MakeSlotsValidAction(const ValueTree& track, TracksState &tracks, ViewState &view) : tracks(tracks) {
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
                setSlotActions.add(new SetProcessorSlotAction(track, processor, newSlot, tracks, view));
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

        struct SetProcessorSlotAction : public UndoableAction {
            SetProcessorSlotAction(const ValueTree &track, const ValueTree& processor, int newSlot,
                                   TracksState &tracks, ViewState &view)
                    : processor(processor), oldSlot(processor[IDs::processorSlot]), newSlot(newSlot) {
                if (!TracksState::isMixerChannelProcessor(this->processor) && this->newSlot >= tracks.getMixerChannelSlotForTrack(track)) {
                    addProcessorRowAction = std::make_unique<AddProcessorRowAction>(track, tracks, view);
                    this->newSlot = tracks.getMixerChannelSlotForTrack(track);
                }
            }

            bool perform() override {
                if (addProcessorRowAction != nullptr)
                    addProcessorRowAction->perform();
                processor.setProperty(IDs::processorSlot, newSlot, nullptr);
                return true;
            }

            bool undo() override {
                processor.setProperty(IDs::processorSlot, oldSlot, nullptr);
                if (addProcessorRowAction != nullptr)
                    addProcessorRowAction->undo();
                return true;
            }

        private:
            ValueTree processor;
            int oldSlot, newSlot;

            struct AddProcessorRowAction : public UndoableAction {
                AddProcessorRowAction(const ValueTree &track, TracksState &tracks, ViewState &view)
                        : track(track), tracks(tracks), view(view) {}

                bool perform() override {
                    if (TracksState::isMasterTrack(track)) {
                        view.getState().setProperty(IDs::numMasterProcessorSlots, view.getNumMasterProcessorSlots() + 1, nullptr);
                        auto mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(track);
                        if (mixerChannelProcessor.isValid())
                            mixerChannelProcessor.setProperty(IDs::processorSlot, int(mixerChannelProcessor[IDs::processorSlot]) + 1, nullptr);
                    } else {
                        view.getState().setProperty(IDs::numProcessorSlots, view.getNumTrackProcessorSlots() + 1, nullptr);
                        for (const auto &nonMasterTrack : tracks.getState()) {
                            if (TracksState::isMasterTrack(nonMasterTrack))
                                continue;
                            auto mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(nonMasterTrack);
                            if (mixerChannelProcessor.isValid())
                                mixerChannelProcessor.setProperty(IDs::processorSlot, int(mixerChannelProcessor[IDs::processorSlot]) + 1, nullptr);
                        }
                    }
                    return true;
                }

                bool undo() override {
                    if (TracksState::isMasterTrack(track)) {
                        auto mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(track);
                        if (mixerChannelProcessor.isValid())
                            mixerChannelProcessor.setProperty(IDs::processorSlot, int(mixerChannelProcessor[IDs::processorSlot]) - 1, nullptr);

                        view.getState().setProperty(IDs::numMasterProcessorSlots, view.getNumMasterProcessorSlots() - 1, nullptr);
                    } else {
                        for (const auto &nonMasterTrack : tracks.getState()) {
                            if (TracksState::isMasterTrack(nonMasterTrack))
                                continue;
                            auto mixerChannelProcessor = tracks.getMixerChannelProcessorForTrack(nonMasterTrack);
                            if (mixerChannelProcessor.isValid())
                                mixerChannelProcessor.setProperty(IDs::processorSlot, int(mixerChannelProcessor[IDs::processorSlot]) - 1, nullptr);
                        }
                        view.getState().setProperty(IDs::numProcessorSlots, view.getNumTrackProcessorSlots() - 1, nullptr);
                    }
                    return true;
                }

            private:
                const ValueTree &track;

                TracksState &tracks;
                ViewState &view;
            };

            std::unique_ptr<AddProcessorRowAction> addProcessorRowAction;
        };

        OwnedArray<SetProcessorSlotAction> setSlotActions;

        JUCE_DECLARE_NON_COPYABLE(MakeSlotsValidAction)
    };

    MakeSlotsValidAction createMakeSlotsValidAction(const ValueTree& track, TracksState &tracks, ViewState &view) {
        addOrMoveProcessorAction.perform();
        return { track, tracks, view };
    }

    AddOrMoveProcessorAction addOrMoveProcessorAction;
    MakeSlotsValidAction makeSlotsValidAction;
};
