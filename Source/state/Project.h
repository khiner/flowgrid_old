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
#include <actions/DuplicateSelectedItemsAction.h>
#include <actions/SelectRectangleAction.h>

#include "PluginManager.h"
#include "Utilities.h"
#include "StatefulAudioProcessorContainer.h"
#include "state/TracksState.h"
#include "state/ConnectionsState.h"
#include "state/ViewState.h"
#include "actions/SelectTrackAction.h"

class Project : public Stateful, public FileBasedDocument, public StatefulAudioProcessorContainer,
                private ChangeListener, private ValueTree::Listener {
public:
    Project(UndoManager &undoManager, PluginManager& pluginManager, AudioDeviceManager& deviceManager)
            : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
              undoManager(undoManager),
              pluginManager(pluginManager),
              tracks(view, pluginManager, undoManager),
              connections(*this, tracks),
              input(tracks, connections, *this, pluginManager, undoManager, deviceManager),
              output(tracks, connections, *this, pluginManager, undoManager, deviceManager),
              deviceManager(deviceManager) {
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

    ValueTree& getState() override { return state; }

    void loadFromState(const ValueTree& newState) override {
        clear();

        view.loadFromState(newState.getChildWithName(IDs::VIEW_STATE));

        const String& inputDeviceName = newState.getChildWithName(IDs::INPUT)[IDs::deviceName];
        const String& outputDeviceName = newState.getChildWithName(IDs::OUTPUT)[IDs::deviceName];

        // TODO this should be replaced with the greyed-out IO processor behavior (keeping connections)
        static const String& failureMessage = TRANS("Could not open an Audio IO device used by this project.  "
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

    void setStatefulAudioProcessorContainer(StatefulAudioProcessorContainer* statefulAudioProcessorContainer) {
        this->statefulAudioProcessorContainer = statefulAudioProcessorContainer;
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        if (statefulAudioProcessorContainer != nullptr)
            return statefulAudioProcessorContainer->getProcessorWrapperForNodeId(nodeId);

        return nullptr;
    }

    AudioProcessorGraph::Node* getNodeForId(AudioProcessorGraph::NodeID nodeId) const override {
        if (statefulAudioProcessorContainer != nullptr)
            return statefulAudioProcessorContainer->getNodeForId(nodeId);

        return nullptr;
    }

    void onProcessorCreated(const ValueTree& processor) override {
        if (statefulAudioProcessorContainer != nullptr)
            statefulAudioProcessorContainer->onProcessorCreated(processor);
    }

    void onProcessorDestroyed(const ValueTree& processor) override {
        if (statefulAudioProcessorContainer != nullptr)
            statefulAudioProcessorContainer->onProcessorDestroyed(processor);
    }

    TracksState& getTracks() { return tracks; }

    ConnectionsState& getConnections() { return connections; }

    ViewState& getView() { return view; }

    InputState& getInput() { return input; }

    OutputState& getOutput() { return output; }

    UndoManager& getUndoManager() { return undoManager; }

    AudioDeviceManager& getDeviceManager() { return deviceManager; }

    bool isPush2MidiInputProcessorConnected() const {
        return connections.isNodeConnected(getNodeIdForState(input.getPush2MidiInputProcessor()));
    }

    bool isShiftHeld() const { return shiftHeld || push2ShiftHeld; }

    void setShiftHeld(bool shiftHeld) {
        if (!isShiftHeld() && shiftHeld)
            startRectangleSelection();
        this->shiftHeld = shiftHeld;
        if (!isShiftHeld())
            endRectangleSelection();
    }

    void setPush2ShiftHeld(bool shiftHeld) {
        if (!isShiftHeld() && shiftHeld)
            startRectangleSelection();
        this->push2ShiftHeld = shiftHeld;
        if (!isShiftHeld())
            endRectangleSelection();
    }

    void createAndAddMasterTrack() {
        undoManager.beginNewTransaction();
        ValueTree masterTrack = doCreateAndAddMasterTrack();
        setTrackSelected(masterTrack, true, true);
    }

    void createTrack(bool addMixer=true) {
        undoManager.beginNewTransaction();
        undoManager.perform(new CreateTrackAction(addMixer, {}, tracks, connections, view));
        if (addMixer)
            undoManager.perform(new CreateProcessorAction(MixerChannelProcessor::getPluginDescription(),
                                                          tracks.indexOf(mostRecentlyCreatedTrack),
                                                          tracks, view, *this));
        setTrackSelected(mostRecentlyCreatedTrack, true, true);
        updateAllDefaultConnections(true);
    }

    // Assumes we're always creating processors to the currently focused track (which is true as of now!)
    void createProcessor(const PluginDescription& description, int slot= -1) {
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
        undoManager.perform(new DeleteSelectedItemsAction(tracks, connections, *this));
        if (view.getFocusedTrackIndex() >= tracks.getNumTracks() && tracks.getNumTracks() > 0)
            setTrackSelected(tracks.getTrack(tracks.getNumTracks() - 1), true, true);
        updateAllDefaultConnections(false);
    }

    void duplicateSelectedItems() {
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();
        undoManager.beginNewTransaction();
        undoManager.perform(new DuplicateSelectedItemsAction(tracks, connections, view, input, *this));
        updateAllDefaultConnections(false);
    }

    void beginDragging(const juce::Point<int> trackAndSlot) {
        initialDraggingTrackAndSlot = trackAndSlot;
        currentlyDraggingTrackAndSlot = initialDraggingTrackAndSlot;

        // During drag actions, everything _except the audio graph_ is updated as a preview
        statefulAudioProcessorContainer->pauseAudioGraphUpdates();
    }

    void dragToPosition(juce::Point<int> trackAndSlot) {
         if (!isCurrentlyDraggingProcessor())
            return;

         if (initialDraggingTrackAndSlot.y == -1) // track-move only
            trackAndSlot.y = -1;

        if (trackAndSlot == currentlyDraggingTrackAndSlot)
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
            undoManager.perform(new MoveSelectedItemsAction(initialDraggingTrackAndSlot, trackAndSlot, isShiftHeld(),
                                                            tracks, connections, view, input, output, *this))) {
            currentlyDraggingTrackAndSlot = trackAndSlot;
        }
    }

    void endDraggingProcessor() {
        if (!isCurrentlyDraggingProcessor())
            return;

        initialDraggingTrackAndSlot = {-1, -1};
        statefulAudioProcessorContainer->resumeAudioGraphUpdatesAndApplyDiffSincePause();
    }

    bool isCurrentlyDraggingProcessor() {
        return initialDraggingTrackAndSlot.x != -1 || initialDraggingTrackAndSlot.y != -1;
    }

    void selectProcessorSlot(const ValueTree& track, int slot, bool deselectOthers=true) {
        selectionEndTrackAndSlot = {tracks.indexOf(track), slot};
        if (selectionStartTrackAndSlot.x != -1 && selectionStartTrackAndSlot.y != -1) { // shift+select
            undoManager.perform(new SelectRectangleAction(selectionStartTrackAndSlot, selectionEndTrackAndSlot, tracks, connections, view, input, *this));
        } else {
            setProcessorSlotSelected(track, slot, true, deselectOthers);
        }
    }

    void setProcessorSlotSelected(const ValueTree& track, int slot, bool selected, bool deselectOthers=true) {
        undoManager.perform(new SelectProcessorSlotAction(track, slot, selected, deselectOthers,
                                                          tracks, connections, view, input, *this));
    }

    void setTrackSelected(const ValueTree& track, bool selected, bool deselectOthers) {
        undoManager.perform(new SelectTrackAction(track, selected, deselectOthers, tracks, connections, view, input, *this));
    }

    void selectProcessor(const ValueTree& processor) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], true);
    }

    bool addConnection(const AudioProcessorGraph::Connection& connection) {
        undoManager.beginNewTransaction();
        ConnectionType connectionType = connection.source.isMIDI() ? midi : audio;
        const auto& sourceProcessor = getProcessorStateForNodeId(connection.source.nodeID);
        // disconnect default outgoing
        undoManager.perform(new DisconnectProcessorAction(connections, sourceProcessor, connectionType, true, false, false, true));
        if (undoManager.perform(new CreateConnectionAction(connection, false, connections, *this))) {
            resetDefaultExternalInputs();
            return true;
        }
        return false;
    }

    bool removeConnection(const AudioProcessorGraph::Connection& connection) {
        undoManager.beginNewTransaction();
        const ValueTree &connectionState = connections.getConnectionMatching(connection);
        if (!connectionState[IDs::isCustomConnection] && isShiftHeld())
            return false; // no default connection stuff while shift is held

        bool removed = undoManager.perform(new DeleteConnectionAction(connectionState, true, true, connections));
        if (removed && connectionState.hasProperty(IDs::isCustomConnection)) {
            const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto sourceNodeId = StatefulAudioProcessorContainer::getNodeIdForState(sourceState);
            const auto& processor = getProcessorStateForNodeId(sourceNodeId);
            updateDefaultConnectionsForProcessor(processor);
            resetDefaultExternalInputs();
        }
        return removed;
    }

    bool disconnectCustom(const ValueTree& processor) {
        undoManager.beginNewTransaction();
        return doDisconnectNode(processor, all, false, true, true, true);
    }

    bool disconnectProcessor(const ValueTree& processor) {
        undoManager.beginNewTransaction();
        doDisconnectNode(processor, all, true, true, true, true);
    }

    void setDefaultConnectionsAllowed(const ValueTree& processor, bool defaultConnectionsAllowed) {
        undoManager.beginNewTransaction();
        undoManager.perform(new SetDefaultConnectionsAllowedAction(processor, defaultConnectionsAllowed, connections));
        resetDefaultExternalInputs();
    }

    void toggleProcessorBypass(ValueTree processor) {
        undoManager.beginNewTransaction();
        processor.setProperty(IDs::bypassed, !processor[IDs::bypassed], &undoManager);
    }

    void navigateUp() { selectTrackAndSlot(findItemToSelectWithUpDownDelta(-1)); }

    void navigateDown() { selectTrackAndSlot(findItemToSelectWithUpDownDelta(1)); }

    void navigateLeft() { selectTrackAndSlot(findItemToSelectWithLeftRightDelta(-1)); }

    void navigateRight() { selectTrackAndSlot(findItemToSelectWithLeftRightDelta(1)); }

    bool canNavigateUp() const { return findItemToSelectWithUpDownDelta(-1).x != -1; }

    bool canNavigateDown() const { return findItemToSelectWithUpDownDelta(1).x != -1; }

    bool canNavigateLeft() const { return findItemToSelectWithLeftRightDelta(-1).x != -1; }

    bool canNavigateRight() const { return findItemToSelectWithLeftRightDelta(1).x != -1; }

    void selectTrackAndSlot(juce::Point<int> trackAndSlot) {
        if (trackAndSlot.x < 0 || trackAndSlot.x >= tracks.getNumTracks())
            return;

        const auto& track = tracks.getTrack(trackAndSlot.x);
        const int slot = trackAndSlot.y;
        if (slot != -1)
            selectProcessorSlot(track, slot);
        else
            setTrackSelected(track, true, true);
    }

    void createDefaultProject() {
        view.initializeDefault();
        input.initializeDefault();
        output.initializeDefault();
        doCreateAndAddMasterTrack();
        createTrack();
        doCreateAndAddProcessor(SineBank::getPluginDescription(), mostRecentlyCreatedTrack, 0);
        resetDefaultExternalInputs(); // Select action only does this if the focused track changes, so we just need to do this once ourselves
        undoManager.clearUndoHistory();
        sendChangeMessage();
    }

    void addPluginsToMenu(PopupMenu& menu, const ValueTree& track) const {
        StringArray disabledPluginIds;

        PopupMenu internalSubMenu;
        PopupMenu externalSubMenu;

        getUserCreatablePluginListInternal().addToMenu(internalSubMenu, pluginManager.getPluginSortMethod(), disabledPluginIds);
        getPluginListExternal().addToMenu(externalSubMenu, pluginManager.getPluginSortMethod(), {}, String(), getUserCreatablePluginListInternal().getNumTypes());

        menu.addSubMenu("Internal", internalSubMenu, true);
        menu.addSeparator();
        menu.addSubMenu("External", externalSubMenu, true);
    }

    KnownPluginList &getUserCreatablePluginListInternal() const {
        return pluginManager.getUserCreatablePluginListInternal();
    }

    KnownPluginList &getPluginListExternal() const {
        return pluginManager.getKnownPluginListExternal();
    }

    KnownPluginList::SortMethod getPluginSortMethod() const {
        return pluginManager.getPluginSortMethod();
    }

    AudioPluginFormatManager& getFormatManager() const {
        return pluginManager.getFormatManager();
    }

    const PluginDescription* getChosenType(const int menuId) const {
        return pluginManager.getChosenType(menuId);
    }

    std::unique_ptr<PluginDescription> getDescriptionForIdentifier(const String &identifier) const {
        return pluginManager.getDescriptionForIdentifier(identifier);
    }

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
        const ValueTree& newState = Utilities::loadValueTree(file, true);
        if (!newState.isValid() || !newState.hasType(IDs::PROJECT))
            return Result::fail(TRANS("Not a valid project file"));

        loadFromState(newState);
        return Result::ok();
    }

    bool isDeviceWithNamePresent(const String& deviceName) const {
        for (auto* deviceType : deviceManager.getAvailableDeviceTypes()) {
            // Input devices
            for (const auto& existingDeviceName : deviceType->getDeviceNames(true)) {
                if (deviceName == existingDeviceName)
                    return true;
            }
            // Output devices
            for (const auto& existingDeviceName : deviceType->getDeviceNames()) {
                if (deviceName == existingDeviceName)
                    return true;
            }
        }
        return false;
    }

    Result saveDocument(const File &file) override {
        for (const auto& track : tracks.getState())
            for (auto processorState : track)
                saveProcessorStateInformationToState(processorState);

        if (Utilities::saveValueTree(state, file, true))
            return Result::ok();

        return Result::fail(TRANS("Could not save the project file"));
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

    AudioDeviceManager& deviceManager;

    StatefulAudioProcessorContainer* statefulAudioProcessorContainer {};
    juce::Point<int> selectionStartTrackAndSlot{-1, -1}, selectionEndTrackAndSlot{-1, -1};

    bool shiftHeld { false }, push2ShiftHeld { false };

    juce::Point<int> initialDraggingTrackAndSlot, currentlyDraggingTrackAndSlot;

    ValueTree mostRecentlyCreatedTrack, mostRecentlyCreatedProcessor;

    void doCreateAndAddProcessor(const PluginDescription &description, ValueTree& track, int slot=-1) {
        if (PluginManager::isGeneratorOrInstrument(&description) &&
            pluginManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            undoManager.perform(new CreateTrackAction(false, track, tracks, connections, view));
            doCreateAndAddProcessor(description, mostRecentlyCreatedTrack, slot);
        }

        if (slot == -1)
            undoManager.perform(new CreateProcessorAction(description, tracks.indexOf(track), tracks, view, *this));
        else
            undoManager.perform(new CreateProcessorAction(description, tracks.indexOf(track), slot, tracks, view, *this));

        selectProcessor(mostRecentlyCreatedProcessor);
        updateAllDefaultConnections();
    }

    ValueTree doCreateAndAddMasterTrack() {
        if (tracks.getMasterTrack().isValid())
            return {}; // only one master track allowed!

        ValueTree masterTrack(IDs::TRACK);
        masterTrack.setProperty(IDs::isMasterTrack, true, nullptr);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        masterTrack.setProperty(IDs::selected, false, nullptr);
        masterTrack.setProperty(IDs::selectedSlotsMask, BigInteger().toString(2), nullptr);

        // TODO should be done via action
        tracks.getState().appendChild(masterTrack, nullptr);

        doCreateAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack);

        return masterTrack;
    }
    
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &undoManager) {
            // if there is nothing to undo, there is nothing to save!
            setChangedFlag(undoManager.canUndo());
        }
    }

    void startRectangleSelection() {
        const juce::Point<int> focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto& focusedTrack = tracks.getTrack(focusedTrackAndSlot.x);
        selectionStartTrackAndSlot = focusedTrack[IDs::selected] ? juce::Point(focusedTrackAndSlot.x, -1) : focusedTrackAndSlot;
    }

    void endRectangleSelection() {
        selectionStartTrackAndSlot = {-1, -1};
        selectionEndTrackAndSlot = {-1, -1};
    }

    juce::Point<int> findItemToSelectWithLeftRightDelta(int delta) const {
        if (view.isGridPaneFocused())
            return findTrackAndSlotToFocusWithGridDelta(delta, 0);
        else
            return findSelectionPaneItemToSelectWithLeftRightDelta(delta);
    }

    juce::Point<int> findItemToSelectWithUpDownDelta(int delta) const {
        if (view.isGridPaneFocused())
            return findTrackAndSlotToFocusWithGridDelta(0, delta);
        else
            return findSelectionPaneItemToFocusWithUpDownDelta(delta);
    }

    // Returns point with x == trackIndex and y == slot.
    // If given position is off the grid, x = -1 is returned
    juce::Point<int> findTrackAndSlotToFocusWithGridDelta(int xDelta, int yDelta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto& focusedTrack = tracks.getTrack(focusedTrackAndSlot.x);
        bool masterIsFocused = TracksState::isMasterTrack(focusedTrack);
        int slotDelta = masterIsFocused ? xDelta : yDelta;

        if (focusedTrack[IDs::selected] && slotDelta < 0)
            // up when track is selected does nothing
            return {-1, -1};
        else if (focusedTrack[IDs::selected] && slotDelta > 0)
            // down when track is selected selects the first slot
            return {focusedTrackAndSlot.x, 0};

        const auto fromGridPosition = trackAndSlotToGridPosition(focusedTrack, focusedTrackAndSlot.y);
        const auto toGridPosition = fromGridPosition + juce::Point(xDelta, yDelta);
        if (toGridPosition.y > view.getNumTrackProcessorSlots())
            return {-1, -1};

        int trackIndex, slot;
        if (toGridPosition.y == view.getNumTrackProcessorSlots()) {
            trackIndex = tracks.indexOf(tracks.getMasterTrack());
            slot = toGridPosition.x + view.getMasterViewSlotOffset() - view.getGridViewTrackOffset();
        } else {
            trackIndex = toGridPosition.x;
            if (trackIndex >= tracks.getNumNonMasterTracks()) {
                if (masterIsFocused)
                    trackIndex = tracks.getNumNonMasterTracks() - 1;
                else
                    return {-1, -1};
            }
            slot = toGridPosition.y + view.getGridViewSlotOffset();
        }

        if (trackIndex < 0 || slot < -1 || slot >= view.getNumAvailableSlotsForTrack(tracks.getTrack(trackIndex)))
            return {-1, -1};

        return {trackIndex, slot};
    }

    juce::Point<int> trackAndSlotToGridPosition(const ValueTree& track, int slot) const {
        if (TracksState::isMasterTrack(track))
            return {slot + view.getGridViewTrackOffset() - view.getMasterViewSlotOffset(), view.getNumTrackProcessorSlots()};
        else
            return {tracks.indexOf(track), slot};
    }

    juce::Point<int> findSelectionPaneItemToFocusWithUpDownDelta(int delta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto& focusedTrack = tracks.getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return {-1, -1};

        const ValueTree& focusedProcessor = TracksState::getProcessorAtSlot(focusedTrack, focusedTrackAndSlot.y);
        if (delta > 0 && focusedTrack[IDs::selected])
            // down when track is selected deselects the track
            return {focusedTrackAndSlot.x, focusedTrackAndSlot.y};
        ValueTree siblingProcessorToSelect;
        if (focusedProcessor.isValid()) {
             siblingProcessorToSelect = focusedProcessor.getSibling(delta);
        } else { // no focused processor - selection is on empty slot
            for (int slot = focusedTrackAndSlot.y + delta; (delta < 0 ? slot >= 0 : slot < view.getNumAvailableSlotsForTrack(focusedTrack)); slot += delta) {
                siblingProcessorToSelect = TracksState::getProcessorAtSlot(focusedTrack, slot);
                if (siblingProcessorToSelect.isValid())
                    break;
            }
        }
        if (siblingProcessorToSelect.isValid())
            return {focusedTrackAndSlot.x, siblingProcessorToSelect[IDs::processorSlot]};
        else if (delta < 0)
            return {focusedTrackAndSlot.x, -1};
        else
            return {-1, -1};
    }

    juce::Point<int> findSelectionPaneItemToSelectWithLeftRightDelta(int delta) const {
        const juce::Point<int> focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree& focusedTrack = tracks.getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return {-1, -1};

        const auto& siblingTrackToSelect = focusedTrack.getSibling(delta);
        if (!siblingTrackToSelect.isValid())
            return {-1, -1};

        int siblingTrackIndex = tracks.indexOf(siblingTrackToSelect);

        if (focusedTrack[IDs::selected] || focusedTrack.getNumChildren() == 0)
            return {siblingTrackIndex, -1};

        if (focusedTrackAndSlot.y != -1) {
            const auto& processorToSelect = tracks.findProcessorNearestToSlot(siblingTrackToSelect, focusedTrackAndSlot.y);
            if (processorToSelect.isValid())
                return {siblingTrackIndex, focusedTrackAndSlot.y};
            else
                return {siblingTrackIndex, -1};
        }

        return {-1, -1};
    }

    bool doDisconnectNode(const ValueTree& processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo={}) {
        return undoManager.perform(new DisconnectProcessorAction(connections, processor, connectionType, defaults,
                                                                 custom, incoming, outgoing, excludingRemovalTo));
    }

    void updateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom=false) {
        undoManager.perform(new UpdateAllDefaultConnectionsAction(makeInvalidDefaultsIntoCustom, true, tracks, connections, input, output, *this));
    }

    void resetDefaultExternalInputs() {
        undoManager.perform(new ResetDefaultExternalInputConnectionsAction(true, connections, tracks, input, *this));
    }

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom=false) {
        undoManager.perform(new UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, output, *this));
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK))
            mostRecentlyCreatedTrack = child;
        else if (child.hasType(IDs::PROCESSOR))
            mostRecentlyCreatedProcessor = child;
    }
};
