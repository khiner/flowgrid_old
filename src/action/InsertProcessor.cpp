#include "InsertProcessor.h"

InsertProcessor::InsertProcessor(juce::Point<int> derivedFromTrackAndSlot, int toTrackIndex, int toSlot, Tracks &tracks, View &view)
        : addOrMoveProcessorAction(derivedFromTrackAndSlot, toTrackIndex, toSlot, tracks, view) {}

InsertProcessor::InsertProcessor(const PluginDescription &description, int toTrackIndex, int toSlot, Tracks &tracks, View &view)
        : addOrMoveProcessorAction(description, toTrackIndex, toSlot, tracks, view) {}

InsertProcessor::InsertProcessor(const PluginDescription &description, int toTrackIndex, Tracks &tracks, View &view)
        : addOrMoveProcessorAction(description, toTrackIndex, tracks, view) {}

bool InsertProcessor::perform() {
    return addOrMoveProcessorAction.perform();
}

bool InsertProcessor::undo() {
    return addOrMoveProcessorAction.undo();
}

InsertProcessor::SetProcessorSlotAction::SetProcessorSlotAction(int trackIndex, int processorIndex, int oldSlot, int newSlot, Tracks &tracks, View &view)
        : trackIndex(trackIndex), processorIndex(processorIndex), oldSlot(oldSlot), newSlot(newSlot), tracks(tracks) {
    const auto *track = tracks.getChild(trackIndex);
    int numNewRows = this->newSlot + 1 - view.getNumProcessorSlots(track != nullptr && track->isMaster());
    for (int i = 0; i < numNewRows; i++)
        addProcessorRowActions.add(new AddProcessorRowAction(trackIndex, tracks, view));
    if (addProcessorRowActions.isEmpty()) {
        auto *conflictingProcessor = track != nullptr ? track->getProcessorAtSlot(newSlot) : nullptr;
        if (conflictingProcessor != nullptr)
            pushConflictingProcessorAction = std::make_unique<SetProcessorSlotAction>(trackIndex, conflictingProcessor->getIndex(), newSlot, newSlot + 1, tracks, view);
    }
}

bool InsertProcessor::SetProcessorSlotAction::perform() {
    for (auto *addProcessorRowAction : addProcessorRowActions)
        addProcessorRowAction->perform();
    if (pushConflictingProcessorAction)
        pushConflictingProcessorAction->perform();
    const auto *track = tracks.getChild(trackIndex);
    track->getProcessorLane()->getChild(processorIndex)->setSlot(newSlot);
    return true;
}

bool InsertProcessor::SetProcessorSlotAction::undo() {
    tracks.getProcessorAt(trackIndex, newSlot)->setSlot(oldSlot);
    if (pushConflictingProcessorAction)
        pushConflictingProcessorAction->undo();
    for (auto *addProcessorRowAction : addProcessorRowActions)
        addProcessorRowAction->undo();
    return true;
}

InsertProcessor::SetProcessorSlotAction::AddProcessorRowAction::AddProcessorRowAction(int trackIndex, Tracks &tracks, View &view)
        : trackIndex(trackIndex), tracks(tracks), view(view) {}

bool InsertProcessor::SetProcessorSlotAction::AddProcessorRowAction::perform() {
    const auto *track = tracks.getChild(trackIndex);
    view.addProcessorSlots(1, track != nullptr && track->isMaster());
    return true;
}

bool InsertProcessor::SetProcessorSlotAction::AddProcessorRowAction::undo() {
    const auto *track = tracks.getChild(trackIndex);
    view.addProcessorSlots(-1, track != nullptr && track->isMaster());
    return true;
}

InsertProcessor::AddOrMoveProcessorAction::AddOrMoveProcessorAction(juce::Point<int> derivedFromTrackAndSlot, int newTrackIndex, int newSlot, Tracks &tracks, View &view)
        : oldTrackIndex(derivedFromTrackAndSlot.getX()), newTrackIndex(newTrackIndex),
          oldSlot(derivedFromTrackAndSlot.getY()), newSlot(newSlot), oldIndex(tracks.getProcessorAt(derivedFromTrackAndSlot)->getIndex()),
          newIndex(tracks.getChild(newTrackIndex)->getInsertIndexForProcessor(tracks.getProcessorAt(derivedFromTrackAndSlot), tracks.getChild(oldTrackIndex)->getProcessorLane(), this->newSlot)),
          setProcessorSlotAction(std::make_unique<SetProcessorSlotAction>(newTrackIndex, newIndex, derivedFromTrackAndSlot.getY(), newSlot, tracks, view)),
          tracks(tracks) {}

InsertProcessor::AddOrMoveProcessorAction::AddOrMoveProcessorAction(const PluginDescription &description, int newTrackIndex, int newSlot, Tracks &tracks, View &view)
        : description(description), oldTrackIndex(-1), newTrackIndex(newTrackIndex),
          oldSlot(-1), newSlot(newSlot), oldIndex(-1),
          newIndex(tracks.getChild(newTrackIndex)->getInsertIndexForProcessor(nullptr, nullptr, newSlot)),
          setProcessorSlotAction(std::make_unique<SetProcessorSlotAction>(newTrackIndex, newIndex, oldSlot, newSlot, tracks, view)),
          tracks(tracks) {}

InsertProcessor::AddOrMoveProcessorAction::AddOrMoveProcessorAction(const PluginDescription &description, int newTrackIndex, Tracks &tracks, View &view)
        : description(description), oldTrackIndex(-1), newTrackIndex(newTrackIndex), oldSlot(-1), newSlot(-1), oldIndex(-1), newIndex(-1), tracks(tracks) {}

bool InsertProcessor::AddOrMoveProcessorAction::perform() {
    auto *derivedFromProcessor = tracks.getProcessorAt(oldTrackIndex, oldSlot);
    const auto state = derivedFromProcessor != nullptr ? derivedFromProcessor->getState() : Processor::initState(description);
    auto *newTrack = tracks.getChild(newTrackIndex);
    if (newSlot == -1) {
        newTrack->getState().appendChild(state, nullptr);
        return true;
    }

    const auto *oldTrack = tracks.getChild(oldTrackIndex);
    auto *newLane = newTrack->getProcessorLane();
    if (oldTrack == nullptr) {
        // only inserting, not moving from another track
        newLane->add(state, newIndex);
    } else {
        const auto oldLane = oldTrack->getProcessorLane();
        if (oldLane == newLane)
            newLane->getState().moveChild(oldIndex, newIndex, nullptr);
        else
            newLane->getState().moveChildFromParent(oldLane->getState(), oldIndex, newIndex, nullptr);
    }
    if (setProcessorSlotAction != nullptr) setProcessorSlotAction->perform();

    return true;
}

bool InsertProcessor::AddOrMoveProcessorAction::undo() {
    if (setProcessorSlotAction != nullptr) setProcessorSlotAction->undo();

    auto *newTrack = tracks.getChild(newTrackIndex);
    if (newSlot == -1) {
        newTrack->getState().removeChild(newTrack->getState().getNumChildren() - 1, nullptr);
        return true;
    }

    auto *newLane = newTrack->getProcessorLane();
    auto *newProcessor = newLane->getChild(newIndex);
    const auto *oldTrack = tracks.getChild(oldTrackIndex);
    if (oldTrack == nullptr) {
        // only inserting, not moving from another track
        newLane->remove(newProcessor->getIndex());
    } else {
        auto oldLane = oldTrack->getProcessorLane();
        if (oldLane == newLane)
            newLane->getState().moveChild(newIndex, oldIndex, nullptr);
        else
            oldLane->getState().moveChildFromParent(newLane->getState(), newIndex, oldIndex, nullptr);
    }

    return true;
}
