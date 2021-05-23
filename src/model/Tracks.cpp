#include "Tracks.h"

Tracks::Tracks(View &view, PluginManager &pluginManager, UndoManager &undoManager)
        : view(view), pluginManager(pluginManager), undoManager(undoManager) {}

void Tracks::loadFromState(const ValueTree &fromState) {
    Stateful<Tracks>::loadFromState(fromState);
    // Re-save all non-string value types,
    // since type information is not saved in XML
    // Also, re-set some vars just to trigger the event (like selected slot mask)
    for (auto track : state) {
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


Array<ValueTree> Tracks::findAllSelectedItems() const {
    Array<ValueTree> selectedItems;
    for (const auto &track : state) {
        if (Track::isSelected(track))
            selectedItems.add(track);
        else
            selectedItems.addArray(Track::findSelectedProcessors(track));
    }
    return selectedItems;
}

juce::Point<int> Tracks::gridPositionToTrackAndSlot(const juce::Point<int> gridPosition, bool allowUpFromMaster) const {
    if (gridPosition.y > view.getNumProcessorSlots()) return INVALID_TRACK_AND_SLOT;

    int trackIndex, slot;
    if (gridPosition.y == view.getNumProcessorSlots()) {
        trackIndex = indexOfTrack(getMasterTrack());
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

    if (trackIndex < 0 || slot < -1 || slot >= getNumSlotsForTrack(getTrack(trackIndex)))
        return INVALID_TRACK_AND_SLOT;

    return {trackIndex, slot};
}

int Tracks::findSlotAt(const juce::Point<int> position, const ValueTree &track) const {
    bool isMaster = Track::isMaster(track);
    int length = isMaster ? position.x : (position.y - View::TRACK_LABEL_HEIGHT);
    if (length < 0) return -1;

    int processorSlotSize = isMaster ? view.getTrackWidth() : view.getProcessorHeight();
    int slot = getSlotOffsetForTrack(track) + length / processorSlotSize;
    return std::clamp(slot, 0, getNumSlotsForTrack(track) - 1);
}
