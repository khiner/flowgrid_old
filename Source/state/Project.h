#pragma once

#include <state/InputState.h>
#include <state/OutputState.h>
#include <actions/SelectProcessorSlotAction.h>
#include <actions/CreateTrackAction.h>
#include <actions/CreateProcessorAction.h>
#include <actions/DeleteSelectedItemsAction.h>
#include <actions/CreateConnectionAction.h>
#include <actions/ResetDefaultExternalInputConnectionsAction.h>
#include <actions/UpdateProcessorDefaultConnectionsAction.h>
#include <actions/SetDefaultConnectionsAllowedAction.h>
#include <actions/UpdateAllDefaultConnectionsAction.h>
#include <actions/MoveSelectedItemsAction.h>
#include <actions/SelectRectangleAction.h>
#include <actions/InsertAction.h>

#include "PluginManager.h"
#include "Utilities.h"
#include "StatefulAudioProcessorContainer.h"
#include "state/TracksState.h"
#include "state/ConnectionsState.h"
#include "state/ViewState.h"
#include "actions/SelectTrackAction.h"
#include "CopiedState.h"

class Project : public Stateful, public FileBasedDocument, public StatefulAudioProcessorContainer,
                private ChangeListener, private ValueTree::Listener {
public:
    Project(UndoManager &undoManager, PluginManager &pluginManager, AudioDeviceManager &deviceManager)
            : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
              undoManager(undoManager),
              pluginManager(pluginManager),
              view(undoManager),
              tracks(view, pluginManager, undoManager),
              connections(*this, tracks),
              input(tracks, connections, *this, pluginManager, undoManager, deviceManager),
              output(tracks, connections, *this, pluginManager, undoManager, deviceManager),
              deviceManager(deviceManager),
              copiedState(tracks, connections, *this) {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        state.appendChild(input.getState(), nullptr);
        state.appendChild(output.getState(), nullptr);
        state.appendChild(tracks.getState(), nullptr);
        state.appendChild(connections.getState(), nullptr);
        state.appendChild(view.getState(), nullptr);
        undoManager.addChangeListener(this);
        tracks.addListener(this);
    }

    ~Project() override {
        tracks.removeListener(this);
    }

    ValueTree &getState() override { return state; }

    void loadFromState(const ValueTree &newState) override {
        clear();

        view.loadFromState(newState.getChildWithName(IDs::VIEW_STATE));

        const String &inputDeviceName = newState.getChildWithName(IDs::INPUT)[IDs::deviceName];
        const String &outputDeviceName = newState.getChildWithName(IDs::OUTPUT)[IDs::deviceName];

        // TODO this should be replaced with the greyed-out IO processor behavior (keeping connections)
        static const String &failureMessage = TRANS("Could not open an Audio IO device used by this project.  "
                                                    "All connections with the missing device will be gone.  "
                                                    "If you want this project to look like it did when you saved it, "
                                                    "the best thing to do is to reconnect the missing device and "
                                                    "reload this project (without saving first!).");

        if (isDeviceWithNamePresent(inputDeviceName))
            input.loadFromState(newState.getChildWithName(IDs::INPUT));
        else
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open input device \"") + inputDeviceName + "\"", failureMessage);

        if (isDeviceWithNamePresent(outputDeviceName))
            output.loadFromState(newState.getChildWithName(IDs::OUTPUT));
        else
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open output device \"") + outputDeviceName + "\"", failureMessage);

        tracks.loadFromState(newState.getChildWithName(IDs::TRACKS));
        connections.loadFromState(newState.getChildWithName(IDs::CONNECTIONS));
        selectProcessor(tracks.getFocusedProcessor());
        undoManager.clearUndoHistory();
        sendChangeMessage();
    }

    void clear() override {
        input.clear();
        output.clear();
        tracks.clear();
        connections.clear();
        undoManager.clearUndoHistory();
    }

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

    void createTrack(bool isMaster, bool addMixer) {
        if (isMaster && tracks.getMasterTrack().isValid())
            return; // only one master track allowed!

        setShiftHeld(false); // prevent rectangle-select behavior when doing cmd+shift+t
        undoManager.beginNewTransaction();

        // If a track is focused, insert the new track to the left of it if there's no mixer,
        // or at the end if the new track has a mixer.
        auto derivedFromTrack = !isMaster && !addMixer && tracks.getFocusedTrack().isValid() &&
                                !tracks.isMasterTrack(tracks.getFocusedTrack()) ? tracks.getFocusedTrack() : ValueTree();
        undoManager.perform(new CreateTrackAction(isMaster, addMixer, derivedFromTrack, tracks, view));
        if (addMixer)
            undoManager.perform(new CreateProcessorAction(MixerChannelProcessor::getPluginDescription(),
                                                          tracks.indexOf(mostRecentlyCreatedTrack), tracks, view, *this));
        setTrackSelected(mostRecentlyCreatedTrack, true);
        updateAllDefaultConnections();
    }

    // Assumes we're always creating processors to the currently focused track (which is true as of now!)
    void createProcessor(const PluginDescription &description, int slot = -1) {
        undoManager.beginNewTransaction();
        auto focusedTrack = tracks.getFocusedTrack();
        if (focusedTrack.isValid()) {
            doCreateAndAddProcessor(description, focusedTrack, slot);
        }
    }

    void deleteSelectedItems() {
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();

        undoManager.beginNewTransaction();
        undoManager.perform(new DeleteSelectedItemsAction(tracks, connections, *statefulAudioProcessorContainer));
        if (view.getFocusedTrackIndex() >= tracks.getNumTracks() && tracks.getNumTracks() > 0)
            setTrackSelected(tracks.getTrack(tracks.getNumTracks() - 1), true);
        updateAllDefaultConnections();
    }

    void copySelectedItems() {
        copiedState.copySelectedItems();
    }

    bool hasCopy() {
        return copiedState.getState().isValid();
    }

    void insert() {
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();
        undoManager.beginNewTransaction();
        undoManager.perform(new InsertAction(false, copiedState.getState(), view.getFocusedTrackAndSlot(), tracks, connections, view, input, *this));
        updateAllDefaultConnections();
    }

    void duplicateSelectedItems() {
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();
        CopiedState duplicateState(tracks, connections, *this);
        duplicateState.copySelectedItems();

        undoManager.beginNewTransaction();
        undoManager.perform(new InsertAction(true, duplicateState.getState(), view.getFocusedTrackAndSlot(), tracks, connections, view, input, *this));
        updateAllDefaultConnections();
    }

    void beginDragging(const juce::Point<int> trackAndSlot) {
        if (trackAndSlot == TracksState::INVALID_TRACK_AND_SLOT ||
            (trackAndSlot.y == -1 && TracksState::isMasterTrack(tracks.getTrack(trackAndSlot.x))))
            return;

        initialDraggingTrackAndSlot = trackAndSlot;
        currentlyDraggingTrackAndSlot = initialDraggingTrackAndSlot;

        // During drag actions, everything _except the audio graph_ is updated as a preview
        statefulAudioProcessorContainer->pauseAudioGraphUpdates();
    }

    void dragToPosition(juce::Point<int> trackAndSlot) {
        if (!isCurrentlyDraggingProcessor() || trackAndSlot == currentlyDraggingTrackAndSlot ||
            (currentlyDraggingTrackAndSlot.y == -1 && trackAndSlot.x == currentlyDraggingTrackAndSlot.x) ||
            trackAndSlot == TracksState::INVALID_TRACK_AND_SLOT)
            return;

        if (currentlyDraggingTrackAndSlot == initialDraggingTrackAndSlot)
            undoManager.beginNewTransaction();

        auto onlyMoveActionInCurrentTransaction = [&]() -> bool {
            Array<const UndoableAction *> actionsFound;
            undoManager.getActionsInCurrentTransaction(actionsFound);
            return actionsFound.size() == 1 && dynamic_cast<const MoveSelectedItemsAction *>(actionsFound.getUnchecked(0)) != nullptr;
        };

        if (undoManager.getNumActionsInCurrentTransaction() > 0) {
            if (onlyMoveActionInCurrentTransaction()) {
                undoManager.undoCurrentTransactionOnly();
            } else {
                undoManager.beginNewTransaction();
                initialDraggingTrackAndSlot = currentlyDraggingTrackAndSlot;
            }
        }

        if (trackAndSlot == initialDraggingTrackAndSlot ||
            undoManager.perform(new MoveSelectedItemsAction(initialDraggingTrackAndSlot, trackAndSlot, isAltHeld(),
                                                            tracks, connections, view, input, output, *statefulAudioProcessorContainer))) {
            currentlyDraggingTrackAndSlot = trackAndSlot;
        }
    }

    void endDraggingProcessor() {
        if (!isCurrentlyDraggingProcessor())
            return;

        initialDraggingTrackAndSlot = TracksState::INVALID_TRACK_AND_SLOT;
        statefulAudioProcessorContainer->resumeAudioGraphUpdatesAndApplyDiffSincePause();
    }

    bool isCurrentlyDraggingProcessor() {
        return initialDraggingTrackAndSlot != TracksState::INVALID_TRACK_AND_SLOT;
    }

    void setProcessorSlotSelected(const ValueTree &track, int slot, bool selected, bool deselectOthers = true) {
        if (!track.isValid())
            return;

        SelectAction *selectAction = nullptr;
        if (selected) {
            const juce::Point<int> trackAndSlot(tracks.indexOf(track), slot);
            if (push2ShiftHeld || shiftHeld)
                selectAction = new SelectRectangleAction(selectionStartTrackAndSlot, trackAndSlot, tracks, connections, view, input, *statefulAudioProcessorContainer);
            else
                selectionStartTrackAndSlot = trackAndSlot;
        }
        if (selectAction == nullptr) {
            if (slot == -1)
                selectAction = new SelectTrackAction(track, selected, deselectOthers, tracks, connections, view, input, *this);
            else
                selectAction = new SelectProcessorSlotAction(track, slot, selected, selected && deselectOthers, tracks, connections, view, input, *statefulAudioProcessorContainer);
        }
        undoManager.perform(selectAction);
    }

    void setTrackSelected(const ValueTree &track, bool selected, bool deselectOthers = true) {
        setProcessorSlotSelected(track, -1, selected, deselectOthers);
    }

    void selectProcessor(const ValueTree &processor) {
        setProcessorSlotSelected(TracksState::getTrackForProcessor(processor), processor[IDs::processorSlot], true);
    }

    bool addConnection(const AudioProcessorGraph::Connection &connection) {
        undoManager.beginNewTransaction();
        ConnectionType connectionType = connection.source.isMIDI() ? midi : audio;
        const auto &sourceProcessor = getProcessorStateForNodeId(connection.source.nodeID);
        // disconnect default outgoing
        undoManager.perform(new DisconnectProcessorAction(connections, sourceProcessor, connectionType, true, false, false, true));
        if (undoManager.perform(new CreateConnectionAction(connection, false, connections, *this))) {
            resetDefaultExternalInputs();
            return true;
        }
        return false;
    }

    bool removeConnection(const AudioProcessorGraph::Connection &connection) {
        undoManager.beginNewTransaction();
        const ValueTree &connectionState = connections.getConnectionMatching(connection);
        if (!connectionState[IDs::isCustomConnection] && isShiftHeld())
            return false; // no default connection stuff while shift is held

        bool removed = undoManager.perform(new DeleteConnectionAction(connectionState, true, true, connections));
        if (removed && connectionState.hasProperty(IDs::isCustomConnection)) {
            const auto &sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto sourceNodeId = StatefulAudioProcessorContainer::getNodeIdForState(sourceState);
            const auto &processor = getProcessorStateForNodeId(sourceNodeId);
            updateDefaultConnectionsForProcessor(processor);
            resetDefaultExternalInputs();
        }
        return removed;
    }

    bool disconnectCustom(const ValueTree &processor) {
        undoManager.beginNewTransaction();
        return doDisconnectNode(processor, all, false, true, true, true);
    }

    bool disconnectProcessor(const ValueTree &processor) {
        undoManager.beginNewTransaction();
        return doDisconnectNode(processor, all, true, true, true, true);
    }

    void setDefaultConnectionsAllowed(const ValueTree &processor, bool defaultConnectionsAllowed) {
        undoManager.beginNewTransaction();
        undoManager.perform(new SetDefaultConnectionsAllowedAction(processor, defaultConnectionsAllowed, connections));
        resetDefaultExternalInputs();
    }

    void toggleProcessorBypass(ValueTree processor) {
        undoManager.beginNewTransaction();
        processor.setProperty(IDs::bypassed, !processor[IDs::bypassed], &undoManager);
    }

    void navigateUp() { selectTrackAndSlot(tracks.trackAndSlotWithUpDownDelta(-1)); }

    void navigateDown() { selectTrackAndSlot(tracks.trackAndSlotWithUpDownDelta(1)); }

    void navigateLeft() { selectTrackAndSlot(tracks.trackAndSlotWithLeftRightDelta(-1)); }

    void navigateRight() { selectTrackAndSlot(tracks.trackAndSlotWithLeftRightDelta(1)); }

    bool canNavigateUp() const { return tracks.trackAndSlotWithUpDownDelta(-1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateDown() const { return tracks.trackAndSlotWithUpDownDelta(1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateLeft() const { return tracks.trackAndSlotWithLeftRightDelta(-1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

    bool canNavigateRight() const { return tracks.trackAndSlotWithLeftRightDelta(1).x != TracksState::INVALID_TRACK_AND_SLOT.x; }

    void selectTrackAndSlot(juce::Point<int> trackAndSlot) {
        if (trackAndSlot.x < 0 || trackAndSlot.x >= tracks.getNumTracks())
            return;

        const auto &track = tracks.getTrack(trackAndSlot.x);
        const int slot = trackAndSlot.y;
        if (slot != -1)
            setProcessorSlotSelected(track, slot, true);
        else
            setTrackSelected(track, true);
    }

    void createDefaultProject() {
        view.initializeDefault();
        input.initializeDefault();
        output.initializeDefault();
        createTrack(true, true);
        createTrack(false, true);
        doCreateAndAddProcessor(SineBank::getPluginDescription(), mostRecentlyCreatedTrack, 0);
        resetDefaultExternalInputs(); // Select action only does this if the focused track changes, so we just need to do this once ourselves
        undoManager.clearUndoHistory();
        sendChangeMessage();
    }

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

    Result loadDocument(const File &file) override {
        const ValueTree &newState = Utilities::loadValueTree(file, true);
        if (!newState.isValid() || !newState.hasType(IDs::PROJECT))
            return Result::fail(TRANS("Not a valid project file"));

        loadFromState(newState);
        return Result::ok();
    }

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

    void doCreateAndAddProcessor(const PluginDescription &description, ValueTree &track, int slot = -1) {
        if (PluginManager::isGeneratorOrInstrument(&description) &&
            tracks.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            undoManager.perform(new CreateTrackAction(false, false, track, tracks, view));
            return doCreateAndAddProcessor(description, mostRecentlyCreatedTrack, slot);
        }

        if (slot == -1)
            undoManager.perform(new CreateProcessorAction(description, tracks.indexOf(track), tracks, view, *statefulAudioProcessorContainer));
        else
            undoManager.perform(new CreateProcessorAction(description, tracks.indexOf(track), slot, tracks, view, *statefulAudioProcessorContainer));

        selectProcessor(mostRecentlyCreatedProcessor);
        updateAllDefaultConnections();
    }

    void changeListenerCallback(ChangeBroadcaster *source) override {
        if (source == &undoManager) {
            // if there is nothing to undo, there is nothing to save!
            setChangedFlag(undoManager.canUndo());
        }
    }

    bool doDisconnectNode(const ValueTree &processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo = {}) {
        return undoManager.perform(new DisconnectProcessorAction(connections, processor, connectionType, defaults,
                                                                 custom, incoming, outgoing, excludingRemovalTo));
    }

    void updateAllDefaultConnections() {
        undoManager.perform(new UpdateAllDefaultConnectionsAction(false, true, tracks, connections, input, output, *statefulAudioProcessorContainer));
    }

    void resetDefaultExternalInputs() {
        undoManager.perform(new ResetDefaultExternalInputConnectionsAction(connections, tracks, input, *statefulAudioProcessorContainer));
    }

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom = false) {
        undoManager.perform(new UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, output, *statefulAudioProcessorContainer));
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK))
            mostRecentlyCreatedTrack = child;
        else if (child.hasType(IDs::PROCESSOR))
            mostRecentlyCreatedProcessor = child;
    }
};
