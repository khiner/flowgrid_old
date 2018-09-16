#pragma once

#include "JuceHeader.h"
#include "Identifiers.h"

class ViewStateManager {
public:
    ViewStateManager() {
        viewState = ValueTree(IDs::VIEW_STATE);
    };

    ValueTree& getState() { return viewState; }

    void initializeDefault() {
        viewState.setProperty(IDs::controlMode, noteControlMode, nullptr);
        viewState.setProperty(IDs::focusedPane, editorPaneName, nullptr);
        viewState.setProperty(IDs::numProcessorSlots, 7, nullptr);
        viewState.setProperty(IDs::numMasterProcessorSlots, 8, nullptr);
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

    int getNumAvailableSlotsForTrack(const ValueTree &track) const {
        return track.hasProperty(IDs::isMasterTrack) ? getNumMasterProcessorSlots() : getNumTrackProcessorSlots();
    }

    int getSlotOffsetForTrack(const ValueTree& track) const {
        return track.hasProperty(IDs::isMasterTrack) ? getMasterViewSlotOffset() : getGridViewSlotOffset();
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

    void togglePaneFocus() {
        if (isGridPaneFocused())
            focusOnEditorPane();
        else
            focusOnGridPane();
    }

    const String sessionControlMode = "session", noteControlMode = "note";
    const String gridPaneName = "grid", editorPaneName = "editor";
private:
    ValueTree viewState;
};
