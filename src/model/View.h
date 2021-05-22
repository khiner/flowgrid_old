#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Stateful.h"

namespace ViewIDs {
#define ID(name) const juce::Identifier name(#name);
ID(VIEW_STATE)
ID(controlMode)
ID(focusedPane)
ID(focusedTrackIndex)
ID(focusedProcessorSlot)
ID(gridViewSlotOffset)
ID(gridViewTrackOffset)
ID(masterViewSlotOffset)
ID(numMasterProcessorSlots)
ID(numProcessorSlots)
#undef ID
}

struct View : public Stateful<View> {
    explicit View(UndoManager &undoManager) : undoManager(undoManager) {}

    static Identifier getIdentifier() { return ViewIDs::VIEW_STATE; }

    void initializeDefault();

    int getFocusedTrackIndex() const { return state[ViewIDs::focusedTrackIndex]; }
    int getFocusedProcessorSlot() const { return state[ViewIDs::focusedProcessorSlot]; }
    juce::Point<int> getFocusedTrackAndSlot() const;
    int getTrackWidth() const { return trackWidth; }
    int getProcessorHeight() const { return processorHeight; }
    int getGridViewTrackOffset() const { return state[ViewIDs::gridViewTrackOffset]; }
    int getGridViewSlotOffset() const { return state[ViewIDs::gridViewSlotOffset]; }
    int getMasterViewSlotOffset() const { return state[ViewIDs::masterViewSlotOffset]; }
    int getNumProcessorSlots(bool isMaster = false) const { return state[isMaster ? ViewIDs::numMasterProcessorSlots : ViewIDs::numProcessorSlots]; }
    int getSlotOffset(bool isMaster) const { return isMaster ? getMasterViewSlotOffset() : getGridViewSlotOffset(); }
    int getProcessorSlotSize(bool isMaster) const { return isMaster ? getTrackWidth() : getProcessorHeight(); }

    bool isInNoteMode() const { return state[ViewIDs::controlMode] == noteControlMode; }
    bool isInSessionMode() const { return state[ViewIDs::controlMode] == sessionControlMode; }
    bool isGridPaneFocused() const { return state[ViewIDs::focusedPane] == gridPaneName; }
    bool isTrackInView(int trackIndex, bool isMaster) const { return isMaster || (trackIndex >= getGridViewTrackOffset() && trackIndex < getGridViewTrackOffset() + NUM_VISIBLE_TRACKS); }
    bool isProcessorSlotInView(int trackIndex, bool isMaster, int slot) const {
        return slot >= getSlotOffset(isMaster) &&
               slot < getSlotOffset(isMaster) + NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + (isMaster ? 1 : 0) &&
               isTrackInView(trackIndex, isMaster);
    }

    void setMasterViewSlotOffset(int masterViewSlotOffset) { state.setProperty(ViewIDs::masterViewSlotOffset, masterViewSlotOffset, &undoManager); }
    void setGridViewSlotOffset(int gridViewSlotOffset) { state.setProperty(ViewIDs::gridViewSlotOffset, gridViewSlotOffset, &undoManager); }
    void setGridViewTrackOffset(int gridViewTrackOffset) { state.setProperty(ViewIDs::gridViewTrackOffset, gridViewTrackOffset, &undoManager); }
    void setTrackWidth(int trackWidth) { this->trackWidth = trackWidth; }
    void setProcessorHeight(int processorHeight) { this->processorHeight = processorHeight; }
    void setNoteMode() { state.setProperty(ViewIDs::controlMode, noteControlMode, nullptr); }
    void setSessionMode() { state.setProperty(ViewIDs::controlMode, sessionControlMode, nullptr); }
    void focusOnGridPane() { state.setProperty(ViewIDs::focusedPane, gridPaneName, nullptr); }
    void focusOnEditorPane() { state.setProperty(ViewIDs::focusedPane, editorPaneName, nullptr); }
    void focusOnTrackIndex(const int trackIndex) { state.setProperty(ViewIDs::focusedTrackIndex, trackIndex, nullptr); }
    void focusOnProcessorSlot(juce::Point<int> slot);
    void addProcessorSlots(int n = 1, bool isMaster = false) {
        state.setProperty(isMaster ? ViewIDs::numMasterProcessorSlots : ViewIDs::numProcessorSlots, getNumProcessorSlots(isMaster) + n, nullptr);
    }
    void updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks);
    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack);

    void togglePaneFocus() {
        if (isGridPaneFocused())
            focusOnEditorPane();
        else
            focusOnGridPane();
    }

    const String sessionControlMode = "session", noteControlMode = "note";
    const String gridPaneName = "grid", editorPaneName = "editor";

    static int getNumVisibleSlots(bool isMaster) { return isMaster ? NUM_VISIBLE_MASTER_TRACK_SLOTS : NUM_VISIBLE_NON_MASTER_TRACK_SLOTS; }

    static constexpr int NUM_VISIBLE_TRACKS = 8,
            NUM_VISIBLE_NON_MASTER_TRACK_SLOTS = 6,
            NUM_VISIBLE_MASTER_TRACK_SLOTS = NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1,
            NUM_VISIBLE_PROCESSOR_SLOTS = NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 3,  // + input row, output row, master track
            TRACK_INPUT_HEIGHT = 34, LANE_HEADER_HEIGHT = 18,
            TRACK_LABEL_HEIGHT = TRACK_INPUT_HEIGHT + LANE_HEADER_HEIGHT,
            TRACKS_MARGIN = 10;
private:
    UndoManager &undoManager;

    int trackWidth{0}, processorHeight{0};
};
