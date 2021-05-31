#include "Tracks.h"

Tracks::Tracks(View &view, UndoManager &undoManager)
        : StatefulList<Track>(state), view(view), undoManager(undoManager) {
    rebuildObjects();
}

void Tracks::loadFromState(const ValueTree &fromState) {
    Stateful<Tracks>::loadFromState(fromState);
    // Re-save all non-string value types,
    // since type information is not saved in XML
    // Also, re-set some vars just to trigger the event (like selected slot mask)
    for (auto track : parent) {
        resetVarToBool(track, TrackIDs::isMaster, nullptr);
        resetVarToBool(track, TrackIDs::selected, nullptr);

        auto lane = Track::getProcessorLane(track);
        lane.sendPropertyChangeMessage(ProcessorLaneIDs::selectedSlotsMask);
        for (auto processor : lane) {
            resetVarToInt(processor, ProcessorIDs::slot, nullptr);
            resetVarToInt(processor, ProcessorIDs::nodeId, nullptr);
            resetVarToInt(processor, ProcessorIDs::initialized, nullptr);
            resetVarToBool(processor, ProcessorIDs::bypassed, nullptr);
            resetVarToBool(processor, ProcessorIDs::acceptsMidi, nullptr);
            resetVarToBool(processor, ProcessorIDs::producesMidi, nullptr);
            resetVarToBool(processor, ProcessorIDs::allowDefaultConnections, nullptr);
        }
    }
}

Array<Track *> Tracks::findAllSelectedTracks() const {
    Array<Track *> selectedTracks;
    for (auto *track : children) {
        if (track->isSelected())
            selectedTracks.add(track);
    }
    return selectedTracks;
}

Array<ValueTree> Tracks::findAllSelectedProcessors() const {
    Array<ValueTree> selectedProcessors;
    for (auto *track : children) {
        if (!track->isSelected())
            selectedProcessors.addArray(track->findSelectedProcessors());
    }
    return selectedProcessors;
}

juce::Point<int> Tracks::gridPositionToTrackAndSlot(const juce::Point<int> gridPosition, bool allowUpFromMaster) const {
    if (gridPosition.y > view.getNumProcessorSlots()) return INVALID_TRACK_AND_SLOT;

    int trackIndex, slot;
    if (gridPosition.y == view.getNumProcessorSlots()) {
        trackIndex = indexOf(getMasterTrack());
        slot = gridPosition.x + view.getMasterViewSlotOffset() - view.getGridViewTrackOffset();
    } else {
        trackIndex = gridPosition.x;
        if (trackIndex < 0 || trackIndex >= getNumNonMasterTracks()) {
            if (allowUpFromMaster)
                // This is annoyingly tied to arrow selection.
                // Allow navigating UP from master track, but not LEFT or RIGHT into no track
                trackIndex = std::clamp(trackIndex, 0, getNumNonMasterTracks() - 1);
            else
                return INVALID_TRACK_AND_SLOT;
        }
        slot = gridPosition.y;
    }

    if (trackIndex < 0 || slot < -1 || slot >= getNumSlotsForTrack(getChild(trackIndex)))
        return INVALID_TRACK_AND_SLOT;

    return {trackIndex, slot};
}

int Tracks::findSlotAt(const juce::Point<int> position, const Track *track) const {
    bool isMaster = track != nullptr && track->isMaster();
    int length = isMaster ? position.x : (position.y - View::TRACK_LABEL_HEIGHT);
    if (length < 0) return -1;

    int processorSlotSize = isMaster ? view.getTrackWidth() : view.getProcessorHeight();
    int slot = view.getSlotOffset(isMaster) + length / processorSlotSize;
    return std::clamp(slot, 0, view.getNumProcessorSlots(isMaster) - 1);
}

bool Tracks::anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType) {
    for (const auto *track : children)
        if (!track->isMaster())
            for (const auto &processor : track->getProcessorLane())
                if (Processor::isProcessorAnEffect(processor, connectionType))
                    return true;
    return false;
}

void Tracks::copySelectedItemsInto(OwnedArray<Track> &copiedTracks, StatefulAudioProcessorWrappers &processorWrappers) {
    copiedTracks.clear();
    for (auto *track : children) {
        auto copiedTrack = track->copy();
        if (track->isSelected()) {
            copiedTrack.getState().copyPropertiesFrom(track->getState(), nullptr);
            for (auto processor : track->getState())
                if (Processor::isType(processor))
                    copiedTrack.getState().appendChild(processorWrappers.copyProcessor(processor), nullptr);
        } else {
            copiedTrack.setMaster(track->isMaster());
        }

        auto copiedLanes = ValueTree(ProcessorLanesIDs::PROCESSOR_LANES);
        for (const auto &lane : track->getProcessorLanes()) {
            auto copiedLane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
            ProcessorLane::setSelectedSlotsMask(copiedLane, ProcessorLane::getSelectedSlotsMask(lane));
            for (auto processor : lane)
                if (track->isSelected() || track->isProcessorSelected(processor))
                    copiedLane.appendChild(processorWrappers.copyProcessor(processor), nullptr);
            copiedLanes.appendChild(copiedLane, nullptr);
        }

        copiedTrack.getState().appendChild(copiedLanes, nullptr);
        copiedTracks.add(std::make_unique<Track>(copiedTrack));
    }
}

juce::Point<int> Tracks::selectionPaneTrackAndSlotWithUpDownDelta(int delta) const {
    const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
    const auto *focusedTrack = getChild(focusedTrackAndSlot.x);
    if (focusedTrack == nullptr) return INVALID_TRACK_AND_SLOT;

    const ValueTree &focusedProcessor = focusedTrack->getProcessorAtSlot(focusedTrackAndSlot.y);
    // down when track is selected deselects the track
    if (delta > 0 && focusedTrack->isSelected()) return {focusedTrackAndSlot.x, focusedTrackAndSlot.y};

    ValueTree siblingProcessorToSelect;
    if (focusedProcessor.isValid()) {
        siblingProcessorToSelect = focusedProcessor.getSibling(delta);
    } else { // no focused processor - selection is on empty slot
        for (int slot = focusedTrackAndSlot.y + delta; (delta < 0 ? slot >= 0 : slot < view.getNumProcessorSlots(focusedTrack->isMaster())); slot += delta) {
            siblingProcessorToSelect = focusedTrack->getProcessorAtSlot(slot);
            if (siblingProcessorToSelect.isValid())
                break;
        }
    }
    if (siblingProcessorToSelect.isValid()) return {focusedTrackAndSlot.x, Processor::getSlot(siblingProcessorToSelect)};
    if (delta < 0) return {focusedTrackAndSlot.x, -1};
    return INVALID_TRACK_AND_SLOT;
}

juce::Point<int> Tracks::selectionPaneTrackAndSlotWithLeftRightDelta(int delta) const {
    const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
    auto *focusedTrack = getChild(focusedTrackAndSlot.x);
    if (focusedTrack == nullptr) return INVALID_TRACK_AND_SLOT;

    auto *siblingTrackToSelect = getChildForState(focusedTrack->getState().getSibling(delta));
    if (siblingTrackToSelect == nullptr) return INVALID_TRACK_AND_SLOT;

    int siblingTrackIndex = indexOf(siblingTrackToSelect);
    const auto &focusedProcessorLane = focusedTrack->getProcessorLane();
    if (focusedTrack->isSelected() || focusedProcessorLane.getNumChildren() == 0) return {siblingTrackIndex, -1};

    if (focusedTrackAndSlot.y != -1) {
        const auto &processorToSelect = siblingTrackToSelect->findProcessorNearestToSlot(focusedTrackAndSlot.y);
        if (processorToSelect.isValid()) return {siblingTrackIndex, focusedTrackAndSlot.y};

        return {siblingTrackIndex, -1};
    }

    return INVALID_TRACK_AND_SLOT;
}

void Tracks::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (isChildType(tree)) {
        auto *track = getChildForState(tree);
        if (track != nullptr) {
            listeners.call([track, i](Listener &l) { l.trackPropertyChanged(track, i); });
        }
    }
}
