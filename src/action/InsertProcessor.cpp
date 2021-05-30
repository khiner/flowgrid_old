#include "InsertProcessor.h"

InsertProcessor::InsertProcessor(const ValueTree &processor, int toTrackIndex, int toSlot, Tracks &tracks, View &view)
        : addOrMoveProcessorAction(processor, toTrackIndex, toSlot, tracks, view) {}

bool InsertProcessor::perform() {
    return addOrMoveProcessorAction.perform();
}

bool InsertProcessor::undo() {
    return addOrMoveProcessorAction.undo();
}

InsertProcessor::SetProcessorSlotAction::SetProcessorSlotAction(int trackIndex, const ValueTree &processor, int newSlot, Tracks &tracks, View &view)
        : processor(processor), oldSlot(Processor::getSlot(processor)), newSlot(newSlot) {
    const auto *track = tracks.getChild(trackIndex);
    int numNewRows = this->newSlot + 1 - view.getNumProcessorSlots(track != nullptr && track->isMaster());
    for (int i = 0; i < numNewRows; i++)
        addProcessorRowActions.add(new AddProcessorRowAction(trackIndex, tracks, view));
    if (addProcessorRowActions.isEmpty()) {
        const auto &conflictingProcessor = track != nullptr ? track->getProcessorAtSlot(newSlot) : ValueTree();
        if (conflictingProcessor.isValid())
            pushConflictingProcessorAction = std::make_unique<SetProcessorSlotAction>(trackIndex, conflictingProcessor, newSlot + 1, tracks, view);
    }
}

bool InsertProcessor::SetProcessorSlotAction::perform() {
    for (auto *addProcessorRowAction : addProcessorRowActions)
        addProcessorRowAction->perform();
    if (pushConflictingProcessorAction)
        pushConflictingProcessorAction->perform();
    if (processor.isValid())
        Processor::setSlot(processor, newSlot);
    return true;
}

bool InsertProcessor::SetProcessorSlotAction::undo() {
    if (processor.isValid())
        Processor::setSlot(processor, oldSlot);
    if (pushConflictingProcessorAction)
        pushConflictingProcessorAction->undo();
    for (auto *addProcessorRowAction : addProcessorRowActions)
        addProcessorRowAction->undo();
    return true;
}

InsertProcessor::SetProcessorSlotAction::AddProcessorRowAction::AddProcessorRowAction(int trackIndex, Tracks &tracks, View &view)
        : trackIndex(trackIndex), tracks(tracks), view(view) {}

bool InsertProcessor::SetProcessorSlotAction::AddProcessorRowAction::perform() {
    const auto &track = tracks.getTrackState(trackIndex);
    if (Track::isMaster(track))
        view.addProcessorSlots(1, true);
    else
        view.addProcessorSlots(1, false);
    return true;
}

bool InsertProcessor::SetProcessorSlotAction::AddProcessorRowAction::undo() {
    const auto &track = tracks.getTrackState(trackIndex);
    if (Track::isMaster(track))
        view.addProcessorSlots(-1, true);
    else
        view.addProcessorSlots(-1, false);
    return true;
}

InsertProcessor::AddOrMoveProcessorAction::AddOrMoveProcessorAction(const ValueTree &processor, int newTrackIndex, int newSlot, Tracks &tracks, View &view)
        : processor(processor), oldTrackIndex(tracks.getTrackIndexForProcessor(processor)), newTrackIndex(newTrackIndex),
          oldSlot(Processor::getSlot(processor)), newSlot(newSlot),
          oldIndex(processor.getParent().indexOf(processor)),
          newIndex(Track::getInsertIndexForProcessor(tracks.getTrackState(newTrackIndex), processor, this->newSlot)),
          setProcessorSlotAction(std::make_unique<SetProcessorSlotAction>(newTrackIndex, processor, newSlot, tracks, view)),
          tracks(tracks) {}

bool InsertProcessor::AddOrMoveProcessorAction::perform() {
    if (processor.isValid()) {
        auto newTrack = tracks.getTrackState(newTrackIndex);
        if (newSlot == -1) {
            newTrack.appendChild(processor, nullptr);
            return true;
        }

        const auto &oldTrack = tracks.getTrackState(oldTrackIndex);
        const auto oldLane = Track::getProcessorLane(oldTrack);
        auto newLane = Track::getProcessorLane(newTrack);

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

bool InsertProcessor::AddOrMoveProcessorAction::undo() {
    if (setProcessorSlotAction != nullptr)
        setProcessorSlotAction->undo();

    if (processor.isValid()) {
        auto newTrack = tracks.getTrackState(newTrackIndex);
        if (newSlot == -1) {
            newTrack.removeChild(processor, nullptr);
            return true;
        }

        const auto &oldTrack = tracks.getTrackState(oldTrackIndex);
        auto oldLane = Track::getProcessorLane(oldTrack);
        auto newLane = Track::getProcessorLane(newTrack);

        if (!oldTrack.isValid()) // only inserting, not moving from another track
            newLane.removeChild(processor, nullptr);
        else if (oldTrack == newTrack)
            newLane.moveChild(newIndex, oldIndex, nullptr);
        else
            oldLane.moveChildFromParent(newLane, newIndex, oldIndex, nullptr);
    }

    return true;
}
