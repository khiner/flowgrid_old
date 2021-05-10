#include "InsertProcessorAction.h"

InsertProcessorAction::InsertProcessorAction(const ValueTree &processor, int toTrackIndex, int toSlot, TracksState &tracks, ViewState &view)
        : addOrMoveProcessorAction(processor, toTrackIndex, toSlot, tracks, view) {}

bool InsertProcessorAction::perform() {
    return addOrMoveProcessorAction.perform();
}

bool InsertProcessorAction::undo() {
    return addOrMoveProcessorAction.undo();
}

InsertProcessorAction::SetProcessorSlotAction::SetProcessorSlotAction(int trackIndex, const ValueTree &processor, int newSlot, TracksState &tracks, ViewState &view)
        : processor(processor), oldSlot(processor[IDs::processorSlot]), newSlot(newSlot) {
    const auto &track = tracks.getTrack(trackIndex);
    int numNewRows = this->newSlot + 1 - view.getNumSlotsForTrack(track);
    for (int i = 0; i < numNewRows; i++)
        addProcessorRowActions.add(new AddProcessorRowAction(trackIndex, tracks, view));
    if (addProcessorRowActions.isEmpty()) {
        const auto &conflictingProcessor = TracksState::getProcessorAtSlot(track, newSlot);
        if (conflictingProcessor.isValid())
            pushConflictingProcessorAction = std::make_unique<SetProcessorSlotAction>(trackIndex, conflictingProcessor, newSlot + 1, tracks, view);
    }
}

bool InsertProcessorAction::SetProcessorSlotAction::perform() {
    for (auto *addProcessorRowAction : addProcessorRowActions)
        addProcessorRowAction->perform();
    if (pushConflictingProcessorAction)
        pushConflictingProcessorAction->perform();
    if (processor.isValid())
        processor.setProperty(IDs::processorSlot, newSlot, nullptr);
    return true;
}

bool InsertProcessorAction::SetProcessorSlotAction::undo() {
    if (processor.isValid())
        processor.setProperty(IDs::processorSlot, oldSlot, nullptr);
    if (pushConflictingProcessorAction)
        pushConflictingProcessorAction->undo();
    for (auto *addProcessorRowAction : addProcessorRowActions)
        addProcessorRowAction->undo();
    return true;
}

InsertProcessorAction::SetProcessorSlotAction::AddProcessorRowAction::AddProcessorRowAction(int trackIndex, TracksState &tracks, ViewState &view)
        : trackIndex(trackIndex), tracks(tracks), view(view) {}

bool InsertProcessorAction::SetProcessorSlotAction::AddProcessorRowAction::perform() {
    const auto &track = tracks.getTrack(trackIndex);
    if (TracksState::isMasterTrack(track))
        view.getState().setProperty(IDs::numMasterProcessorSlots, view.getNumMasterProcessorSlots() + 1, nullptr);
    else
        view.getState().setProperty(IDs::numProcessorSlots, view.getNumTrackProcessorSlots() + 1, nullptr);
    return true;
}

bool InsertProcessorAction::SetProcessorSlotAction::AddProcessorRowAction::undo() {
    const auto &track = tracks.getTrack(trackIndex);
    if (TracksState::isMasterTrack(track))
        view.getState().setProperty(IDs::numMasterProcessorSlots, view.getNumMasterProcessorSlots() - 1, nullptr);
    else
        view.getState().setProperty(IDs::numProcessorSlots, view.getNumTrackProcessorSlots() - 1, nullptr);
    return true;
}

InsertProcessorAction::AddOrMoveProcessorAction::AddOrMoveProcessorAction(const ValueTree &processor, int newTrackIndex, int newSlot, TracksState &tracks, ViewState &view)
        : processor(processor), oldTrackIndex(tracks.indexOf(TracksState::getTrackForProcessor(processor))), newTrackIndex(newTrackIndex),
          oldSlot(processor[IDs::processorSlot]), newSlot(newSlot),
          oldIndex(processor.getParent().indexOf(processor)),
          newIndex(TracksState::getInsertIndexForProcessor(tracks.getTrack(newTrackIndex), processor, this->newSlot)),
          setProcessorSlotAction(std::make_unique<SetProcessorSlotAction>(newTrackIndex, processor, newSlot, tracks, view)),
          tracks(tracks) {}

bool InsertProcessorAction::AddOrMoveProcessorAction::perform() {
    if (processor.isValid()) {
        auto newTrack = tracks.getTrack(newTrackIndex);
        if (newSlot == -1) {
            newTrack.appendChild(processor, nullptr);
            return true;
        }

        const auto &oldTrack = tracks.getTrack(oldTrackIndex);
        const auto oldLane = TracksState::getProcessorLaneForTrack(oldTrack);
        auto newLane = TracksState::getProcessorLaneForTrack(newTrack);

        if (!oldLane.isValid()) // only inserting, not moving from another track
            newLane.addChild(processor, newIndex, nullptr);
        else if (oldLane == newTrack)
            newLane.moveChild(oldIndex, newIndex, nullptr);
        else
            newLane.moveChildFromParent(oldLane, oldIndex, newIndex, nullptr);
    }
    if (setProcessorSlotAction != nullptr)
        setProcessorSlotAction->perform();

    return true;
}

bool InsertProcessorAction::AddOrMoveProcessorAction::undo() {
    if (setProcessorSlotAction != nullptr)
        setProcessorSlotAction->undo();

    if (processor.isValid()) {
        auto newTrack = tracks.getTrack(newTrackIndex);
        if (newSlot == -1) {
            newTrack.removeChild(processor, nullptr);
            return true;
        }

        const auto &oldTrack = tracks.getTrack(oldTrackIndex);
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
