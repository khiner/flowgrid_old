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

    void setMasterViewSlotOffset(int masterViewSlotOffset) {
        state.setProperty(ViewIDs::masterViewSlotOffset, masterViewSlotOffset, &undoManager);
    }

    void setGridViewSlotOffset(int gridViewSlotOffset) {
        state.setProperty(ViewIDs::gridViewSlotOffset, gridViewSlotOffset, &undoManager);
    }

    void setGridViewTrackOffset(int gridViewTrackOffset) {
        state.setProperty(ViewIDs::gridViewTrackOffset, gridViewTrackOffset, &undoManager);
    }

    void updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks);

    // TODO This and TracksState::isProcessorSlitInView are similar
    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack);

    int getFocusedTrackIndex() const { return state[ViewIDs::focusedTrackIndex]; }

    int getFocusedProcessorSlot() const { return state[ViewIDs::focusedProcessorSlot]; }

    juce::Point<int> getFocusedTrackAndSlot() const;

    int getGridViewTrackOffset() const { return state[ViewIDs::gridViewTrackOffset]; }

    int getGridViewSlotOffset() const { return state[ViewIDs::gridViewSlotOffset]; }

    int getMasterViewSlotOffset() const { return state[ViewIDs::masterViewSlotOffset]; }

    int getNumProcessorSlots(bool isMaster = false) const { return state[isMaster ? ViewIDs::numMasterProcessorSlots : ViewIDs::numProcessorSlots]; }

    bool isInNoteMode() const { return state[ViewIDs::controlMode] == noteControlMode; }

    bool isInSessionMode() const { return state[ViewIDs::controlMode] == sessionControlMode; }

    bool isGridPaneFocused() const { return state[ViewIDs::focusedPane] == gridPaneName; }

    void setNoteMode() { state.setProperty(ViewIDs::controlMode, noteControlMode, nullptr); }

    void setSessionMode() { state.setProperty(ViewIDs::controlMode, sessionControlMode, nullptr); }

    void focusOnGridPane() { state.setProperty(ViewIDs::focusedPane, gridPaneName, nullptr); }

    void focusOnEditorPane() { state.setProperty(ViewIDs::focusedPane, editorPaneName, nullptr); }

    void focusOnTrackIndex(const int trackIndex) {
        state.setProperty(ViewIDs::focusedTrackIndex, trackIndex, nullptr);
    }

    void focusOnProcessorSlot(const juce::Point<int> slot) {
        auto currentlyFocusedTrackAndSlot = getFocusedTrackAndSlot();
        focusOnTrackIndex(slot.x);
        if (slot.x != currentlyFocusedTrackAndSlot.x && slot.y == currentlyFocusedTrackAndSlot.y) {
            // Different track but same slot selected - still send out the message
            state.sendPropertyChangeMessage(ViewIDs::focusedProcessorSlot);
        } else {
            state.setProperty(ViewIDs::focusedProcessorSlot, slot.y, nullptr);
        }
    }

    void focusOnProcessorSlot(const ValueTree &track, const int processorSlot) {
        auto trackIndex = track.isValid() ? track.getParent().indexOf(track) : 0;
        focusOnProcessorSlot({trackIndex, processorSlot});
    }

    void addProcessorSlots(int n = 1, bool isMaster = false) {
        state.setProperty(isMaster ? ViewIDs::numMasterProcessorSlots : ViewIDs::numProcessorSlots,
                          getNumProcessorSlots(isMaster) + n, nullptr);
    }


    void togglePaneFocus() {
        if (isGridPaneFocused())
            focusOnEditorPane();
        else
            focusOnGridPane();
    }

    void setTrackWidth(int trackWidth) { this->trackWidth = trackWidth; }

    void setProcessorHeight(int processorHeight) { this->processorHeight = processorHeight; }

    int getTrackWidth() const { return trackWidth; }

    int getProcessorHeight() const { return processorHeight; }

    const String sessionControlMode = "session", noteControlMode = "note";
    const String gridPaneName = "grid", editorPaneName = "editor";

    static constexpr int NUM_VISIBLE_TRACKS = 8,
            NUM_VISIBLE_NON_MASTER_TRACK_SLOTS = 6,
            NUM_VISIBLE_MASTER_TRACK_SLOTS = NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1,
            NUM_VISIBLE_PROCESSOR_SLOTS = NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 3;  // + input row, output row, master track

    static constexpr int TRACK_INPUT_HEIGHT = 34, LANE_HEADER_HEIGHT = 18,
            TRACK_LABEL_HEIGHT = TRACK_INPUT_HEIGHT + LANE_HEADER_HEIGHT,
            TRACKS_MARGIN = 10;
private:
    UndoManager &undoManager;

    int trackWidth{0}, processorHeight{0};
};
