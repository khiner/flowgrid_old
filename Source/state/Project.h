#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "state/TracksState.h"
#include "state/ConnectionsState.h"
#include "state/ViewState.h"
#include "state/InputState.h"
#include "state/OutputState.h"
#include "PluginManager.h"
#include "StatefulAudioProcessorContainer.h"
#include "CopiedState.h"

class Project : public Stateful, public FileBasedDocument, public StatefulAudioProcessorContainer,
                private ChangeListener, private ValueTree::Listener {
public:
    Project(UndoManager &undoManager, PluginManager &pluginManager, AudioDeviceManager &deviceManager);

    ~Project() override;

    ValueTree &getState() override { return state; }

    void loadFromState(const ValueTree &newState) override;

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

    void setStatefulAudioProcessorContainer(StatefulAudioProcessorContainer *statefulAudioProcessorContainer) {
        this->statefulAudioProcessorContainer = statefulAudioProcessorContainer;
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        if (statefulAudioProcessorContainer != nullptr)
            return statefulAudioProcessorContainer->getProcessorWrapperForNodeId(nodeId);

        return nullptr;
    }

    AudioProcessorGraph::Node *getNodeForId(AudioProcessorGraph::NodeID nodeId) const override {
        if (statefulAudioProcessorContainer != nullptr)
            return statefulAudioProcessorContainer->getNodeForId(nodeId);

        return nullptr;
    }

    void onProcessorCreated(const ValueTree &processor) override {
        if (statefulAudioProcessorContainer != nullptr)
            statefulAudioProcessorContainer->onProcessorCreated(processor);
    }

    void onProcessorDestroyed(const ValueTree &processor) override {
        if (statefulAudioProcessorContainer != nullptr)
            statefulAudioProcessorContainer->onProcessorDestroyed(processor);
    }

    TracksState &getTracks() { return tracks; }

    ConnectionsState &getConnections() { return connections; }

    ViewState &getView() { return view; }

    InputState &getInput() { return input; }

    OutputState &getOutput() { return output; }

    UndoManager &getUndoManager() { return undoManager; }

    AudioDeviceManager &getDeviceManager() { return deviceManager; }

    bool isPush2MidiInputProcessorConnected() const {
        return connections.isNodeConnected(getNodeIdForState(input.getPush2MidiInputProcessor()));
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
        copiedState.copySelectedItems();
    }

    bool hasCopy() {
        return copiedState.getState().isValid();
    }

    void insert();

    void duplicateSelectedItems();

    void beginDragging(const juce::Point<int> trackAndSlot);

    void dragToPosition(juce::Point<int> trackAndSlot);

    void endDraggingProcessor() {
        if (!isCurrentlyDraggingProcessor())
            return;

        initialDraggingTrackAndSlot = TracksState::INVALID_TRACK_AND_SLOT;
        statefulAudioProcessorContainer->resumeAudioGraphUpdatesAndApplyDiffSincePause();
    }

    bool isCurrentlyDraggingProcessor() {
        return initialDraggingTrackAndSlot != TracksState::INVALID_TRACK_AND_SLOT;
    }

    void setProcessorSlotSelected(const ValueTree &track, int slot, bool selected, bool deselectOthers = true);

    void setTrackSelected(const ValueTree &track, bool selected, bool deselectOthers = true);

    void selectProcessor(const ValueTree &processor);

    void selectTrackAndSlot(juce::Point<int> trackAndSlot);

    bool addConnection(const AudioProcessorGraph::Connection &connection);

    bool removeConnection(const AudioProcessorGraph::Connection &connection);

    bool disconnectCustom(const ValueTree &processor);

    bool disconnectProcessor(const ValueTree &processor);

    void setDefaultConnectionsAllowed(const ValueTree &processor, bool defaultConnectionsAllowed);

    void toggleProcessorBypass(ValueTree processor);

    void navigateUp() { selectTrackAndSlot(tracks.trackAndSlotWithUpDownDelta(-1)); }

    void navigateDown() { selectTrackAndSlot(tracks.trackAndSlotWithUpDownDelta(1)); }

    void navigateLeft() { selectTrackAndSlot(tracks.trackAndSlotWithLeftRightDelta(-1)); }

    void navigateRight() { selectTrackAndSlot(tracks.trackAndSlotWithLeftRightDelta(1)); }

    bool canNavigateUp() const { return tracks.trackAndSlotWithUpDownDelta(-1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateDown() const { return tracks.trackAndSlotWithUpDownDelta(1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateLeft() const { return tracks.trackAndSlotWithLeftRightDelta(-1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateRight() const { return tracks.trackAndSlotWithLeftRightDelta(1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

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

    bool isDeviceWithNamePresent(const String &deviceName) const {
        for (auto *deviceType : deviceManager.getAvailableDeviceTypes()) {
            // Input devices
            for (const auto &existingDeviceName : deviceType->getDeviceNames(true)) {
                if (deviceName == existingDeviceName)
                    return true;
            }
            // Output devices
            for (const auto &existingDeviceName : deviceType->getDeviceNames()) {
                if (deviceName == existingDeviceName)
                    return true;
            }
        }
        return false;
    }

    Result saveDocument(const File &file) override {
        for (const auto &track : tracks.getState())
            for (auto processorState : TracksState::getProcessorLaneForTrack(track))
                saveProcessorStateInformationToState(processorState);

        if (auto xml = state.createXml())
            if (!xml->writeTo(file))
                return Result::fail(TRANS("Could not save the project file"));

        return Result::ok();
    }

    File getLastDocumentOpened() override {
        RecentlyOpenedFilesList recentFiles;
        recentFiles.restoreFromString(getUserSettings()->getValue("recentProjectFiles"));

        return recentFiles.getFile(0);
    }

    void setLastDocumentOpened(const File &file) override {
        RecentlyOpenedFilesList recentFiles;
        recentFiles.restoreFromString(getUserSettings()->getValue("recentProjectFiles"));
        recentFiles.addFile(file);
        getUserSettings()->setValue("recentProjectFiles", recentFiles.toString());
    }

    static String getFilenameSuffix() { return ".smp"; }

private:
    ValueTree state;

    UndoManager &undoManager;
    PluginManager &pluginManager;
    ViewState view;
    TracksState tracks;
    ConnectionsState connections;
    InputState input;
    OutputState output;

    AudioDeviceManager &deviceManager;

    StatefulAudioProcessorContainer *statefulAudioProcessorContainer{};
    juce::Point<int> selectionStartTrackAndSlot = {0, 0};

    bool shiftHeld{false}, altHeld{false}, push2ShiftHeld{false};

    juce::Point<int> initialDraggingTrackAndSlot = TracksState::INVALID_TRACK_AND_SLOT,
            currentlyDraggingTrackAndSlot = TracksState::INVALID_TRACK_AND_SLOT;

    ValueTree mostRecentlyCreatedTrack, mostRecentlyCreatedProcessor;

    CopiedState copiedState;

    void doCreateAndAddProcessor(const PluginDescription &description, ValueTree &track, int slot = -1);

    void changeListenerCallback(ChangeBroadcaster *source) override;

    bool doDisconnectNode(const ValueTree &processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo = {});

    void updateAllDefaultConnections();

    void resetDefaultExternalInputs();

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom = false);

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK))
            mostRecentlyCreatedTrack = child;
        else if (child.hasType(IDs::PROCESSOR))
            mostRecentlyCreatedProcessor = child;
    }
};
