#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Stateful.h"

namespace ViewStateIDs {
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

class ViewState : public Stateful {
public:

    explicit ViewState(UndoManager &undoManager);

    ValueTree &getState() override { return viewState; }

    void loadFromState(const ValueTree &state) override {
        viewState.copyPropertiesFrom(state, nullptr);
    }

    void initializeDefault();

    void setMasterViewSlotOffset(int masterViewSlotOffset) {
        viewState.setProperty(ViewStateIDs::masterViewSlotOffset, masterViewSlotOffset, &undoManager);
    }

    void setGridViewSlotOffset(int gridViewSlotOffset) {
        viewState.setProperty(ViewStateIDs::gridViewSlotOffset, gridViewSlotOffset, &undoManager);
    }

    void setGridViewTrackOffset(int gridViewTrackOffset) {
        viewState.setProperty(ViewStateIDs::gridViewTrackOffset, gridViewTrackOffset, &undoManager);
    }

    void updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks);

    // TODO This and TracksState::isProcessorSlitInView are similar
    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack);

    int getFocusedTrackIndex() const { return viewState[ViewStateIDs::focusedTrackIndex]; }

    int getFocusedProcessorSlot() const { return viewState[ViewStateIDs::focusedProcessorSlot]; }

    juce::Point<int> getFocusedTrackAndSlot() const;

    int getNumTrackProcessorSlots() const { return viewState[ViewStateIDs::numProcessorSlots]; }

    int getNumMasterProcessorSlots() const { return viewState[ViewStateIDs::numMasterProcessorSlots]; }

    int getGridViewTrackOffset() const { return viewState[ViewStateIDs::gridViewTrackOffset]; }

    int getGridViewSlotOffset() const { return viewState[ViewStateIDs::gridViewSlotOffset]; }

    int getMasterViewSlotOffset() const { return viewState[ViewStateIDs::masterViewSlotOffset]; }

    bool isInNoteMode() const { return viewState[ViewStateIDs::controlMode] == noteControlMode; }

    bool isInSessionMode() const { return viewState[ViewStateIDs::controlMode] == sessionControlMode; }

    bool isGridPaneFocused() const { return viewState[ViewStateIDs::focusedPane] == gridPaneName; }

    void setNoteMode() { viewState.setProperty(ViewStateIDs::controlMode, noteControlMode, nullptr); }

    void setSessionMode() { viewState.setProperty(ViewStateIDs::controlMode, sessionControlMode, nullptr); }

    void focusOnGridPane() { viewState.setProperty(ViewStateIDs::focusedPane, gridPaneName, nullptr); }

    void focusOnEditorPane() { viewState.setProperty(ViewStateIDs::focusedPane, editorPaneName, nullptr); }

    void focusOnTrackIndex(const int trackIndex) {
        viewState.setProperty(ViewStateIDs::focusedTrackIndex, trackIndex, nullptr);
    }

    void focusOnProcessorSlot(const juce::Point<int> slot) {
        auto currentlyFocusedTrackAndSlot = getFocusedTrackAndSlot();
        focusOnTrackIndex(slot.x);
        if (slot.x != currentlyFocusedTrackAndSlot.x && slot.y == currentlyFocusedTrackAndSlot.y) {
            // Different track but same slot selected - still send out the message
            viewState.sendPropertyChangeMessage(ViewStateIDs::focusedProcessorSlot);
        } else {
            viewState.setProperty(ViewStateIDs::focusedProcessorSlot, slot.y, nullptr);
        }
    }

    void focusOnProcessorSlot(const ValueTree &track, const int processorSlot) {
        auto trackIndex = track.isValid() ? track.getParent().indexOf(track) : 0;
        focusOnProcessorSlot({trackIndex, processorSlot});
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
    ValueTree viewState;
    UndoManager &undoManager;

    int trackWidth{0}, processorHeight{0};
};
