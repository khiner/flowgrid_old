#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "model/Tracks.h"
#include "model/Connections.h"
#include "model/View.h"
#include "model/Input.h"
#include "model/Output.h"
#include "PluginManager.h"
#include "ProcessorGraph.h"
#include "CopiedTracks.h"

namespace ProjectIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROJECT)
ID(name)
#undef ID
}

struct Project : public Stateful<Project>, public FileBasedDocument, private ChangeListener, private ValueTree::Listener {
    Project(View &view, Tracks &tracks, Connections &connections, Input &input, Output &output,
            ProcessorGraph &processorGraph, UndoManager &undoManager, PluginManager &pluginManager, AudioDeviceManager &deviceManager);

    ~Project() override;

    static Identifier getIdentifier() { return ProjectIDs::PROJECT; }

    void loadFromState(const ValueTree &fromState) override;

    void clear() override;

    // TODO any way to do all this in the constructor?
    void initialize() {
        const auto &lastOpenedProjectFile = getLastDocumentOpened();
        if (!(lastOpenedProjectFile.exists() && loadFrom(lastOpenedProjectFile, true)))
            newDocument();
        undoManager.clearUndoHistory();
    }

    void undo() {
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();
        undoManager.undo();
    }

    void redo() {
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();
        undoManager.redo();
    }

    UndoManager &getUndoManager() { return undoManager; }

    bool isPush2MidiInputProcessorConnected() const {
        return connections.isNodeConnected(Processor::getNodeId(input.getState().getChildWithProperty(ProcessorIDs::deviceName, Push2MidiDevice::getDeviceName())));
    }

    bool isShiftHeld() const { return shiftHeld || push2ShiftHeld; }

    bool isAltHeld() const { return altHeld; }

    void setShiftHeld(bool shiftHeld) {
        this->shiftHeld = shiftHeld;
    }

    void setAltHeld(bool altHeld) {
        this->altHeld = altHeld;
    }

    void setPush2ShiftHeld(bool push2ShiftHeld) {
        this->push2ShiftHeld = push2ShiftHeld;
    }

    void createTrack(bool isMaster);

    // Assumes we're always creating processors to the currently focused track (which is true as of now!)
    void createProcessor(const PluginDescription &description, int slot = -1);

    void deleteSelectedItems();

    void copySelectedItems() {
        copiedTracks.copySelectedItems();
    }

    bool hasCopy() { return copiedTracks.getState().isValid(); }

    void insert();

    void duplicateSelectedItems();

    void beginDragging(juce::Point<int> trackAndSlot);

    void dragToPosition(juce::Point<int> trackAndSlot);

    void endDraggingProcessor() {
        if (!isCurrentlyDraggingProcessor()) return;

        initialDraggingTrackAndSlot = Tracks::INVALID_TRACK_AND_SLOT;
        processorGraph.resumeAudioGraphUpdatesAndApplyDiffSincePause();
    }

    bool isCurrentlyDraggingProcessor() {
        return initialDraggingTrackAndSlot != Tracks::INVALID_TRACK_AND_SLOT;
    }

    void setProcessorSlotSelected(const ValueTree &track, int slot, bool selected, bool deselectOthers = true);

    void setTrackSelected(const ValueTree &track, bool selected, bool deselectOthers = true);

    void selectProcessor(const ValueTree &processor);

    void selectTrackAndSlot(juce::Point<int> trackAndSlot);

    bool disconnectCustom(const ValueTree &processor);

    void setDefaultConnectionsAllowed(const ValueTree &processor, bool defaultConnectionsAllowed);

    void toggleProcessorBypass(ValueTree processor);

    void navigateUp() { selectTrackAndSlot(tracks.trackAndSlotWithUpDownDelta(-1)); }

    void navigateDown() { selectTrackAndSlot(tracks.trackAndSlotWithUpDownDelta(1)); }

    void navigateLeft() { selectTrackAndSlot(tracks.trackAndSlotWithLeftRightDelta(-1)); }

    void navigateRight() { selectTrackAndSlot(tracks.trackAndSlotWithLeftRightDelta(1)); }

    bool canNavigateUp() const { return tracks.trackAndSlotWithUpDownDelta(-1).x != Tracks::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateDown() const { return tracks.trackAndSlotWithUpDownDelta(1).x != Tracks::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateLeft() const { return tracks.trackAndSlotWithLeftRightDelta(-1).x != Tracks::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateRight() const { return tracks.trackAndSlotWithLeftRightDelta(1).x != Tracks::INVALID_TRACK_AND_SLOT.x; }

    void createDefaultProject();

    PluginManager &getPluginManager() const { return pluginManager; }

    //==============================================================================================================
    void newDocument() {
        clear();
        setFile({});
        createDefaultProject();
    }

    String getDocumentTitle() override {
        if (!getFile().exists())
            return TRANS("Unnamed");

        return getFile().getFileNameWithoutExtension();
    }

    Result loadDocument(const File &file) override;

    bool isDeviceWithNamePresent(const String &deviceName) const;

    Result saveDocument(const File &file) override;

    File getLastDocumentOpened() override;

    void setLastDocumentOpened(const File &file) override;

    static String getFilenameSuffix() { return ".smp"; }

private:
    View &view;
    Tracks &tracks;
    Connections &connections;
    Input &input;
    Output &output;

    ProcessorGraph &processorGraph;
    UndoManager &undoManager;
    PluginManager &pluginManager;
    AudioDeviceManager &deviceManager;

    juce::Point<int> selectionStartTrackAndSlot = {0, 0};

    bool shiftHeld{false}, altHeld{false}, push2ShiftHeld{false};

    juce::Point<int> initialDraggingTrackAndSlot = Tracks::INVALID_TRACK_AND_SLOT,
            currentlyDraggingTrackAndSlot = Tracks::INVALID_TRACK_AND_SLOT;

    ValueTree mostRecentlyCreatedTrack, mostRecentlyCreatedProcessor;

    CopiedTracks copiedTracks;

    void doCreateAndAddProcessor(const PluginDescription &description, ValueTree &track, int slot = -1);

    void changeListenerCallback(ChangeBroadcaster *source) override;

    void updateAllDefaultConnections();

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (Track::isType(child))
            mostRecentlyCreatedTrack = child;
        else if (Processor::isType(child))
            mostRecentlyCreatedProcessor = child;
    }
};
