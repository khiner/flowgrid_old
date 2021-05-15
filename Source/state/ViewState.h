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

    ValueTree &getState() override { return state; }

    static bool isType(const ValueTree &state) { return state.hasType(ViewStateIDs::VIEW_STATE); }

    void loadFromParentState(const ValueTree &parent) override { loadFromState(parent.getChildWithName(ViewStateIDs::VIEW_STATE)); }

    void loadFromState(const ValueTree &fromState) override {
        state.copyPropertiesFrom(fromState, nullptr);
    }

    void initializeDefault();

    void setMasterViewSlotOffset(int masterViewSlotOffset) {
        state.setProperty(ViewStateIDs::masterViewSlotOffset, masterViewSlotOffset, &undoManager);
    }

    void setGridViewSlotOffset(int gridViewSlotOffset) {
        state.setProperty(ViewStateIDs::gridViewSlotOffset, gridViewSlotOffset, &undoManager);
    }

    void setGridViewTrackOffset(int gridViewTrackOffset) {
        state.setProperty(ViewStateIDs::gridViewTrackOffset, gridViewTrackOffset, &undoManager);
    }

    void updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks);

    // TODO This and TracksState::isProcessorSlitInView are similar
    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack);

    int getFocusedTrackIndex() const { return state[ViewStateIDs::focusedTrackIndex]; }

    int getFocusedProcessorSlot() const { return state[ViewStateIDs::focusedProcessorSlot]; }

    juce::Point<int> getFocusedTrackAndSlot() const;

    int getGridViewTrackOffset() const { return state[ViewStateIDs::gridViewTrackOffset]; }

    int getGridViewSlotOffset() const { return state[ViewStateIDs::gridViewSlotOffset]; }

    int getMasterViewSlotOffset() const { return state[ViewStateIDs::masterViewSlotOffset]; }

    int getNumProcessorSlots(bool isMaster = false) const { return state[isMaster ? ViewStateIDs::numMasterProcessorSlots : ViewStateIDs::numProcessorSlots]; }

    bool isInNoteMode() const { return state[ViewStateIDs::controlMode] == noteControlMode; }

    bool isInSessionMode() const { return state[ViewStateIDs::controlMode] == sessionControlMode; }

    bool isGridPaneFocused() const { return state[ViewStateIDs::focusedPane] == gridPaneName; }

    void setNoteMode() { state.setProperty(ViewStateIDs::controlMode, noteControlMode, nullptr); }

    void setSessionMode() { state.setProperty(ViewStateIDs::controlMode, sessionControlMode, nullptr); }

    void focusOnGridPane() { state.setProperty(ViewStateIDs::focusedPane, gridPaneName, nullptr); }

    void focusOnEditorPane() { state.setProperty(ViewStateIDs::focusedPane, editorPaneName, nullptr); }

    void focusOnTrackIndex(const int trackIndex) {
        state.setProperty(ViewStateIDs::focusedTrackIndex, trackIndex, nullptr);
    }

    void focusOnProcessorSlot(const juce::Point<int> slot) {
        auto currentlyFocusedTrackAndSlot = getFocusedTrackAndSlot();
        focusOnTrackIndex(slot.x);
        if (slot.x != currentlyFocusedTrackAndSlot.x && slot.y == currentlyFocusedTrackAndSlot.y) {
            // Different track but same slot selected - still send out the message
            state.sendPropertyChangeMessage(ViewStateIDs::focusedProcessorSlot);
        } else {
            state.setProperty(ViewStateIDs::focusedProcessorSlot, slot.y, nullptr);
        }
    }

    void focusOnProcessorSlot(const ValueTree &track, const int processorSlot) {
        auto trackIndex = track.isValid() ? track.getParent().indexOf(track) : 0;
        focusOnProcessorSlot({trackIndex, processorSlot});
    }

    void addProcessorSlots(int n = 1, bool isMaster = false) {
        state.setProperty(isMaster ? ViewStateIDs::numMasterProcessorSlots : ViewStateIDs::numProcessorSlots,
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
    ValueTree state;
    UndoManager &undoManager;

    int trackWidth{0}, processorHeight{0};
};
