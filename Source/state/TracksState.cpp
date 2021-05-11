#include "TracksState.h"

TracksState::TracksState(ViewState &view, PluginManager &pluginManager, UndoManager &undoManager)
        : view(view), pluginManager(pluginManager), undoManager(undoManager) {
    tracks = ValueTree(IDs::TRACKS);
    tracks.setProperty(IDs::name, "Tracks", nullptr);
}

void TracksState::loadFromState(const ValueTree &state) {
    moveAllChildren(state, getState(), nullptr);

    // Re-save all non-string value types,
    // since type information is not saved in XML
    // Also, re-set some vars just to trigger the event (like selected slot mask)
    for (auto track : tracks) {
        resetVarToBool(track, IDs::isMasterTrack, this);
        resetVarToBool(track, IDs::selected, this);

        auto lane = getProcessorLaneForTrack(track);
        lane.sendPropertyChangeMessage(IDs::selectedSlotsMask);
        for (auto processor : lane) {
            resetVarToInt(processor, IDs::processorSlot, this);
            resetVarToInt(processor, IDs::nodeId, this);
            resetVarToInt(processor, IDs::processorInitialized, this);
            resetVarToBool(processor, IDs::bypassed, this);
            resetVarToBool(processor, IDs::acceptsMidi, this);
            resetVarToBool(processor, IDs::producesMidi, this);
            resetVarToBool(processor, IDs::allowDefaultConnections, this);
        }
    }
}

Array<ValueTree> TracksState::getAllProcessorsForTrack(const ValueTree &track) {
    Array<ValueTree> allProcessors;
    allProcessors.add(getInputProcessorForTrack(track));
    for (const auto &processor : getProcessorLaneForTrack(track)) {
        allProcessors.add(processor);
    }
    allProcessors.add(getOutputProcessorForTrack(track));
    return allProcessors;
}

int TracksState::getInsertIndexForProcessor(const ValueTree &track, const ValueTree &processor, int insertSlot) {
    const auto &lane = getProcessorLaneForTrack(track);

    bool sameLane = lane == getProcessorLaneForProcessor(processor);
    auto handleSameLane = [sameLane](int index) -> int { return sameLane ? std::max(0, index - 1) : index; };
    for (const auto &otherProcessor : lane) {
        int otherSlot = otherProcessor[IDs::processorSlot];
        if (otherSlot >= insertSlot && otherProcessor != processor) {
            int otherIndex = lane.indexOf(otherProcessor);
            return sameLane && lane.indexOf(processor) < otherIndex ? handleSameLane(otherIndex) : otherIndex;
        }
    }
    return handleSameLane(lane.getNumChildren());
}

Array<ValueTree> TracksState::findAllSelectedItems() const {
    Array<ValueTree> selectedItems;
    for (const auto &track : tracks) {
        if (track[IDs::selected])
            selectedItems.add(track);
        else
            selectedItems.addArray(findSelectedProcessorsForTrack(track));
    }
    return selectedItems;
}

ValueTree TracksState::findProcessorNearestToSlot(const ValueTree &track, int slot) {
    const auto &lane = getProcessorLaneForTrack(track);
    auto nearestSlot = INT_MAX;
    ValueTree nearestProcessor;
    for (const auto &processor : lane) {
        int otherSlot = processor[IDs::processorSlot];
        if (otherSlot == slot)
            return processor;
        if (abs(slot - otherSlot) < abs(slot - nearestSlot)) {
            nearestSlot = otherSlot;
            nearestProcessor = processor;
        }
        if (otherSlot > slot)
            break; // processors are ordered by slot.
    }
    return nearestProcessor;
}

juce::Point<int> TracksState::gridPositionToTrackAndSlot(const juce::Point<int> gridPosition, bool allowUpFromMaster) const {
    if (gridPosition.y > view.getNumTrackProcessorSlots())
        return INVALID_TRACK_AND_SLOT;

    int trackIndex, slot;
    if (gridPosition.y == view.getNumTrackProcessorSlots()) {
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

    if (trackIndex < 0 || slot < -1 || slot >= view.getNumSlotsForTrack(getTrack(trackIndex)))
        return INVALID_TRACK_AND_SLOT;

    return {trackIndex, slot};
}
