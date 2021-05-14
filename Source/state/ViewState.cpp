#include "ViewState.h"

ViewState::ViewState(UndoManager &undoManager) : undoManager(undoManager) {
    viewState = ValueTree(ViewStateIDs::VIEW_STATE);
}

void ViewState::initializeDefault() {
    setNoteMode();
    focusOnEditorPane();
    focusOnProcessorSlot({}, -1);
    viewState.setProperty(ViewStateIDs::numProcessorSlots, NUM_VISIBLE_NON_MASTER_TRACK_SLOTS, nullptr);
    // the number of processors in the master track aligns with the number of tracks
    viewState.setProperty(ViewStateIDs::numMasterProcessorSlots, NUM_VISIBLE_MASTER_TRACK_SLOTS, nullptr);
    setGridViewTrackOffset(0);
    setGridViewSlotOffset(0);
    setMasterViewSlotOffset(0);
}

void ViewState::updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks) {
    if (trackIndex < 0)
        return; // invalid
    auto viewTrackOffset = getGridViewTrackOffset();
    if (trackIndex >= viewTrackOffset + NUM_VISIBLE_TRACKS)
        setGridViewTrackOffset(trackIndex - NUM_VISIBLE_TRACKS + 1);
    else if (trackIndex < viewTrackOffset)
        setGridViewTrackOffset(trackIndex);
    else if (numNonMasterTracks - viewTrackOffset < NUM_VISIBLE_TRACKS && numNonMasterTracks >= NUM_VISIBLE_TRACKS)
        // always show last N tracks if available
        setGridViewTrackOffset(numNonMasterTracks - NUM_VISIBLE_TRACKS);
}

void ViewState::updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack) {
    if (processorSlot < 0)
        return; // invalid
    if (isMasterTrack) {
        auto viewSlotOffset = getMasterViewSlotOffset();
        if (processorSlot >= viewSlotOffset + NUM_VISIBLE_MASTER_TRACK_SLOTS)
            setMasterViewSlotOffset(processorSlot - NUM_VISIBLE_MASTER_TRACK_SLOTS + 1);
        else if (processorSlot < viewSlotOffset)
            setMasterViewSlotOffset(processorSlot);
    } else {
        auto viewSlotOffset = getGridViewSlotOffset();
        if (processorSlot >= viewSlotOffset + NUM_VISIBLE_NON_MASTER_TRACK_SLOTS)
            setGridViewSlotOffset(processorSlot - NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1);
        else if (processorSlot < viewSlotOffset)
            setGridViewSlotOffset(processorSlot);
    }
}

juce::Point<int> ViewState::getFocusedTrackAndSlot() const {
    int trackIndex = viewState.hasProperty(ViewStateIDs::focusedTrackIndex) ? getFocusedTrackIndex() : 0;
    int processorSlot = viewState.hasProperty(ViewStateIDs::focusedProcessorSlot) ? getFocusedProcessorSlot() : -1;
    return {trackIndex, processorSlot};
}
