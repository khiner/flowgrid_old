#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Stateful.h"
#include "Identifiers.h"

class ViewState : public Stateful {
public:
    explicit ViewState(UndoManager &undoManager);

    ValueTree &getState() override { return viewState; }

    void loadFromState(const ValueTree &state) override {
        viewState.copyPropertiesFrom(state, nullptr);
    }

    void initializeDefault();

    static bool isMasterTrack(const ValueTree &track) {
        return track.isValid() && track[IDs::isMasterTrack];
    }

    static int getNumVisibleSlotsForTrack(const ValueTree &track) {
        return isMasterTrack(track) ? NUM_VISIBLE_MASTER_TRACK_SLOTS : NUM_VISIBLE_NON_MASTER_TRACK_SLOTS;
    }

    int getSlotOffsetForTrack(const ValueTree &track) const {
        return isMasterTrack(track) ? getMasterViewSlotOffset() : getGridViewSlotOffset();
    }

    int getNumSlotsForTrack(const ValueTree &track) const {
        return isMasterTrack(track) ? getNumMasterProcessorSlots() : getNumTrackProcessorSlots();
    }

    int getProcessorSlotSize(const ValueTree &track) const {
        return isMasterTrack(track) ? getTrackWidth() : getProcessorHeight();
    }

    void setMasterViewSlotOffset(int masterViewSlotOffset) {
        viewState.setProperty(IDs::masterViewSlotOffset, masterViewSlotOffset, &undoManager);
    }

    void setGridViewSlotOffset(int gridViewSlotOffset) {
        viewState.setProperty(IDs::gridViewSlotOffset, gridViewSlotOffset, &undoManager);
    }

    void setGridViewTrackOffset(int gridViewTrackOffset) {
        viewState.setProperty(IDs::gridViewTrackOffset, gridViewTrackOffset, &undoManager);
    }

    void updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks);

    // TODO This and TracksState::isProcessorSlitInView are similar
    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack);

    int getFocusedTrackIndex() const { return viewState[IDs::focusedTrackIndex]; }

    int getFocusedProcessorSlot() const { return viewState[IDs::focusedProcessorSlot]; }

    juce::Point<int> getFocusedTrackAndSlot() const;

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

    void focusOnProcessorSlot(const juce::Point<int> slot) {
        auto currentlyFocusedTrackAndSlot = getFocusedTrackAndSlot();
        focusOnTrackIndex(slot.x);
        if (slot.x != currentlyFocusedTrackAndSlot.x && slot.y == currentlyFocusedTrackAndSlot.y) {
            // Different track but same slot selected - still send out the message
            viewState.sendPropertyChangeMessage(IDs::focusedProcessorSlot);
        } else {
            viewState.setProperty(IDs::focusedProcessorSlot, slot.y, nullptr);
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

    int findSlotAt(juce::Point<int> position, const ValueTree &track) const;

    bool isTrackInView(const ValueTree &track) const {
        if (isMasterTrack(track)) return true;

        auto trackIndex = track.getParent().indexOf(track);
        auto trackViewOffset = getGridViewTrackOffset();
        return trackIndex >= trackViewOffset && trackIndex < trackViewOffset + NUM_VISIBLE_TRACKS;
    }

    bool isProcessorSlotInView(const ValueTree &track, int slot) const {
        bool inView = slot >= getSlotOffsetForTrack(track) &&
                      slot < getSlotOffsetForTrack(track) + NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + (isMasterTrack(track) ? 1 : 0);
        return inView && isTrackInView(track);
    }

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
