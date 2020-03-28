#pragma once

#include "JuceHeader.h"
#include "Identifiers.h"
#include "Stateful.h"

class ViewState : public Stateful {
public:
    ViewState(UndoManager &undoManager) : undoManager(undoManager) {
        viewState = ValueTree(IDs::VIEW_STATE);
    };

    ValueTree &getState() override { return viewState; }

    void loadFromState(const ValueTree &state) override {
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
        viewState.setProperty(IDs::masterViewSlotOffset, masterViewSlotOffset, &undoManager);
    }

    void setGridViewSlotOffset(int gridViewSlotOffset) {
        viewState.setProperty(IDs::gridViewSlotOffset, gridViewSlotOffset, &undoManager);
    }

    void setGridViewTrackOffset(int gridViewTrackOffset) {
        viewState.setProperty(IDs::gridViewTrackOffset, gridViewTrackOffset, &undoManager);
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

    int getFocusedProcessorSlot() const { return viewState[IDs::focusedProcessorSlot]; }

    juce::Point<int> getFocusedTrackAndSlot() const {
        int trackIndex = viewState.hasProperty(IDs::focusedTrackIndex) ? getFocusedTrackIndex() : 0;
        int processorSlot = viewState.hasProperty(IDs::focusedProcessorSlot) ? getFocusedProcessorSlot() : -1;
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

    juce::Point<int> findTrackAndSlotAt(const juce::Point<int> position, int numNonMasterTracks) {
        bool isNonMasterTrack = numNonMasterTracks > 0 && position.y < TRACK_LABEL_HEIGHT + getNumTrackProcessorSlots() * getProcessorHeight();
        int trackIndex, slot;
        if (isNonMasterTrack) {
            trackIndex = std::clamp((position.x - TRACK_LABEL_HEIGHT) / getTrackWidth(), 0, numNonMasterTracks - 1);
            slot = (position.y - TRACK_LABEL_HEIGHT) / getProcessorHeight();
        } else {
            trackIndex = numNonMasterTracks;
            slot = (position.x - TRACK_LABEL_HEIGHT) / getTrackWidth();
        }
        int numAvailableSlots = isNonMasterTrack ? getNumTrackProcessorSlots() : getNumMasterProcessorSlots();
        return {trackIndex, std::clamp(slot, -1, numAvailableSlots - 1)};
    }

    int findSlotAt(const juce::Point<int> position, const ValueTree &track) {
        bool isMasterTrack = track.isValid() && track[IDs::isMasterTrack];
        int length = isMasterTrack ? position.x : position.y;
        if (length < TRACK_LABEL_HEIGHT)
            return -1;

        int nonMixerCellSize = isMasterTrack ? getTrackWidth() : getProcessorHeight();
        int numAvailableSlots = isMasterTrack ? getNumMasterProcessorSlots() : getNumTrackProcessorSlots();
        int slot = (length - TRACK_LABEL_HEIGHT) / nonMixerCellSize;
        return std::clamp(slot, 0, numAvailableSlots - 1);
    }

    const String sessionControlMode = "session", noteControlMode = "note";
    const String gridPaneName = "grid", editorPaneName = "editor";

    static constexpr int NUM_VISIBLE_TRACKS = 8, NUM_VISIBLE_PROCESSOR_SLOTS = 10;
    static constexpr int TRACK_LABEL_HEIGHT = 32;
private:
    ValueTree viewState;
    UndoManager &undoManager;

    int trackWidth{0}, processorHeight{0};
};
