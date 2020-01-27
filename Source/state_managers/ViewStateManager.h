#pragma once

#include "JuceHeader.h"
#include "Identifiers.h"
#include "StateManager.h"

class ViewStateManager : public StateManager {
public:
    ViewStateManager() {
        viewState = ValueTree(IDs::VIEW_STATE);
    };

    ValueTree& getState() override { return viewState; }

    void loadFromState(const ValueTree& state) override {
        viewState.copyPropertiesFrom(state, nullptr);
    }

    void initializeDefault() {
        setNoteMode();
        focusOnEditorPane();
        focusOnProcessorSlot({}, -1);
        // - 3 to make room for: Input row, Output row, master track.
        viewState.setProperty(IDs::numProcessorSlots, NUM_VISIBLE_PROCESSOR_SLOTS - 3, nullptr);
        // the number of processors in the master track aligns with the number of tracks
        viewState.setProperty(IDs::numMasterProcessorSlots, NUM_VISIBLE_TRACKS, nullptr);
        setGridViewTrackOffset(0);
        setGridViewSlotOffset(0);
        setMasterViewSlotOffset(0);
    }

    void setMasterViewSlotOffset(int masterViewSlotOffset) {
        viewState.setProperty(IDs::masterViewSlotOffset, masterViewSlotOffset, nullptr);
    }

    void setGridViewSlotOffset(int gridViewSlotOffset) {
        viewState.setProperty(IDs::gridViewSlotOffset, gridViewSlotOffset, nullptr);
    }

    void setGridViewTrackOffset(int gridViewTrackOffset) {
        viewState.setProperty(IDs::gridViewTrackOffset, gridViewTrackOffset, nullptr);
    }

    void addProcessorRow(UndoManager* undoManager) {
        viewState.setProperty(IDs::numProcessorSlots, getNumTrackProcessorSlots() + 1, undoManager);
    }

    void addMasterProcessorSlot(UndoManager* undoManager) {
        viewState.setProperty(IDs::numMasterProcessorSlots, getNumMasterProcessorSlots() + 1, undoManager);
    }

    int getMixerChannelSlotForTrack(const ValueTree& track) const {
        return getNumAvailableSlotsForTrack(track) - 1;
    }

    // TODO cleanup - shouldn't access IDs::isMasterTrack directly outside of TracksStateManager
    int getNumAvailableSlotsForTrack(const ValueTree &track) const {
        return track.hasProperty(IDs::isMasterTrack) ? getNumMasterProcessorSlots() : getNumTrackProcessorSlots();
    }

    int getSlotOffsetForTrack(const ValueTree& track) const {
        return track.hasProperty(IDs::isMasterTrack) ? getMasterViewSlotOffset() : getGridViewSlotOffset();
    }

    bool isProcessorSlotInView(const ValueTree& track, int correctedSlot) {
        bool inView = correctedSlot >= getSlotOffsetForTrack(track) &&
                      correctedSlot < getSlotOffsetForTrack(track) + NUM_VISIBLE_TRACKS;
        if (track.hasProperty(IDs::isMasterTrack))
            inView &= getGridViewSlotOffset() + NUM_VISIBLE_TRACKS > getNumTrackProcessorSlots();
        else {
            auto trackIndex = track.getParent().indexOf(track);
            auto trackViewOffset = getGridViewTrackOffset();
            inView &= trackIndex >= trackViewOffset && trackIndex < trackViewOffset + NUM_VISIBLE_TRACKS;
        }
        return inView;
    }

    void updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks) {
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

    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack) {
        if (processorSlot < 0)
            return; // invalid
        if (isMasterTrack) {
            auto viewSlotOffset = getMasterViewSlotOffset();
            if (processorSlot >= viewSlotOffset + NUM_VISIBLE_TRACKS)
                setMasterViewSlotOffset(processorSlot - NUM_VISIBLE_TRACKS + 1);
            else if (processorSlot < viewSlotOffset)
                setMasterViewSlotOffset(processorSlot);
            processorSlot = getNumTrackProcessorSlots();
        }

        auto viewSlotOffset = getGridViewSlotOffset();
        if (processorSlot >= viewSlotOffset + NUM_VISIBLE_TRACKS)
            setGridViewSlotOffset(processorSlot - NUM_VISIBLE_TRACKS + 1);
        else if (processorSlot < viewSlotOffset)
            setGridViewSlotOffset(processorSlot);
    }

    int getFocusedTrackIndex() const { return viewState[IDs::focusedTrackIndex]; }

    Point<int> getFocusedTrackAndSlot() const {
        int trackIndex = viewState.hasProperty(IDs::focusedTrackIndex) ? int(viewState[IDs::focusedTrackIndex]) : 0;
        int processorSlot = viewState.hasProperty(IDs::focusedProcessorSlot) ? int(viewState[IDs::focusedProcessorSlot]) : -1;
        return {trackIndex, processorSlot};
    }

    int getNumTrackProcessorSlots() const { return viewState[IDs::numProcessorSlots]; }
    int getNumMasterProcessorSlots() const { return viewState[IDs::numMasterProcessorSlots]; }
    int getGridViewTrackOffset() const { return viewState[IDs::gridViewTrackOffset]; }
    int getGridViewSlotOffset() const { return viewState[IDs::gridViewSlotOffset]; }
    int getMasterViewSlotOffset() const { return viewState[IDs::masterViewSlotOffset]; }

    bool isInNoteMode() const { return viewState[IDs::controlMode] == noteControlMode; }
    bool isInSessionMode() const { return viewState[IDs::controlMode] == sessionControlMode; }
    bool isGridPaneFocused() const { return viewState[IDs::focusedPane] == gridPaneName; }

    void setNoteMode() { viewState.setProperty(IDs::controlMode, noteControlMode, nullptr); }
    void setSessionMode() { viewState.setProperty(IDs::controlMode, sessionControlMode, nullptr); }
    void focusOnGridPane() { viewState.setProperty(IDs::focusedPane, gridPaneName, nullptr); }
    void focusOnEditorPane() { viewState.setProperty(IDs::focusedPane, editorPaneName, nullptr); }

    void focusOnTrackIndex(const int trackIndex) {
        viewState.setProperty(IDs::focusedTrackIndex, trackIndex, nullptr);
    }

    void focusOnProcessorSlot(const ValueTree& track, const int processorSlot) {
        auto trackIndex = track.isValid() ? track.getParent().indexOf(track) : 0;
        auto currentlyFocusedTrackAndSlot = getFocusedTrackAndSlot();
        focusOnTrackIndex(trackIndex);
        if (trackIndex != currentlyFocusedTrackAndSlot.x && processorSlot == currentlyFocusedTrackAndSlot.y) {
            // Different track but same slot selected - still send out the message
            viewState.sendPropertyChangeMessage(IDs::focusedProcessorSlot);
        } else {
            viewState.setProperty(IDs::focusedProcessorSlot, processorSlot, nullptr);
        }

    }

    void togglePaneFocus() {
        if (isGridPaneFocused())
            focusOnEditorPane();
        else
            focusOnGridPane();
    }

    void addFocusStateToUndoStack(UndoManager *undoManager) {
        resetVarAllowingNoopUndo(viewState, IDs::focusedTrackIndex, undoManager);
        resetVarAllowingNoopUndo(viewState, IDs::focusedProcessorSlot, undoManager);
    }

    const String sessionControlMode = "session", noteControlMode = "note";
    const String gridPaneName = "grid", editorPaneName = "editor";

    static constexpr int NUM_VISIBLE_TRACKS = 8, NUM_VISIBLE_PROCESSOR_SLOTS = 10;
private:
    ValueTree viewState;
};
