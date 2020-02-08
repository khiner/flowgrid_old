#include <utility>
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

#pragma once

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
    struct TrackAndSlot {
        TrackAndSlot() : slot(0) {};
        TrackAndSlot(ValueTree track, const ValueTree& processor) : track(std::move(track)), slot(processor[IDs::processorSlot]) {}
        TrackAndSlot(ValueTree track, const int slot) : track(std::move(track)), slot(jmax(-1, slot)) {}
        TrackAndSlot(ValueTree track) : track(std::move(track)), slot(-1) {}

        bool isValid() const { return track.isValid(); }

        void select(Project& project) {
            if (!track.isValid())
                return;

            const auto& focusedTrack = project.getFocusedTrack();
            if (slot != -1)
                project.selectProcessorSlot(track, slot);
            if (slot == -1 || (focusedTrack != track && focusedTrack[IDs::selected]))
                project.setTrackSelected(track, true, true);
        }

        ValueTree track;
        const int slot;
    };

    Project(UndoManager &undoManager, PluginManager& pluginManager, AudioDeviceManager& deviceManager)
            : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
              pluginManager(pluginManager),
              tracks(view, *this, pluginManager, undoManager),
              input(*this, pluginManager),
              output(*this, pluginManager),
              connections(*this, input, output, tracks),
              undoManager(undoManager), deviceManager(deviceManager) {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        state.appendChild(input.getState(), nullptr);
        state.appendChild(output.getState(), nullptr);
        state.appendChild(tracks.getState(), nullptr);
        state.appendChild(connections.getState(), nullptr);
        state.appendChild(view.getState(), nullptr);
        undoManager.addChangeListener(this);
        state.addListener(this);
    }

    ~Project() override {
        state.removeListener(this);
        deviceManager.removeChangeListener(this);
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

    // TODO any way to to all this in the constructor?
    void initialize() {
        const auto &lastOpenedProjectFile = getLastDocumentOpened();
        if (!(lastOpenedProjectFile.exists() && loadFrom(lastOpenedProjectFile, true)))
            newDocument();
        deviceManager.addChangeListener(this);
        changeListenerCallback(&deviceManager);
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
        endRectangleSelection(); // TODO should not need
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

    TracksState& getTracksManager() { return tracks; }

    ConnectionsState& getConnectionStateManager() { return connections; }

    ViewState& getViewStateManager() { return view; }

    UndoManager& getUndoManager() { return undoManager; }

    AudioDeviceManager& getDeviceManager() { return deviceManager; }

    ViewState& getView() { return view; }

    TracksState& getTracks() { return tracks; }

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
        undoManager.beginNewTransaction();
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();
        undoManager.perform(new DeleteSelectedItemsAction(tracks, connections, *this));
        if (view.getFocusedTrackIndex() >= tracks.getNumTracks()) {
            setTrackSelected(tracks.getTrack(tracks.getNumTracks() - 1), true, true);
        } else {
            updateAllDefaultConnections(false);
        }
    }

    void duplicateSelectedItems() {
        if (isCurrentlyDraggingProcessor())
            endDraggingProcessor();
        undoManager.beginNewTransaction();
        undoManager.perform(new DuplicateSelectedItemsAction(tracks, connections, view, input, pluginManager, *this));
        updateAllDefaultConnections(false);
    }

    void beginDraggingProcessor(const ValueTree& processor) {
        currentlyDraggingProcessor = processor;
        initialDraggingTrackAndSlot = {tracks.indexOf(processor.getParent()), processor[IDs::processorSlot]};
        currentlyDraggingTrackAndSlot = initialDraggingTrackAndSlot;

        // During drag actions, everything _except the audio graph_ is updated as a preview
        statefulAudioProcessorContainer->pauseAudioGraphUpdates();
    }

    void dragProcessorToPosition(const juce::Point<int> &trackAndSlot) {
        if (!isCurrentlyDraggingProcessor() || trackAndSlot == currentlyDraggingTrackAndSlot)
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
                                                            tracks, connections, view, input, *this))) {
            currentlyDraggingTrackAndSlot = trackAndSlot;
        }
    }

    void endDraggingProcessor() {
        if (!isCurrentlyDraggingProcessor())
            return;

        currentlyDraggingTrack = {};
        currentlyDraggingProcessor = {};
        statefulAudioProcessorContainer->resumeAudioGraphUpdatesAndApplyDiffSincePause();
    }

    void setCurrentlyDraggingTrack(const ValueTree& currentlyDraggingTrack) {
        this->currentlyDraggingTrack = currentlyDraggingTrack;
    }

    const ValueTree& getCurrentlyDraggingTrack() const {
        return currentlyDraggingTrack;
    }

    bool isCurrentlyDraggingProcessor() {
        return currentlyDraggingProcessor.isValid();
    }

    ValueTree& getCurrentlyDraggingProcessor() {
        return currentlyDraggingProcessor;
    }

    void selectProcessorSlot(const ValueTree& track, int slot, bool deselectOthers=true) {
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track, slot);
        if (selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, slot);
        } else {
            setProcessorSlotSelected(track, slot, true, deselectOthers);
        }
    }

    void deselectProcessorSlot(const ValueTree& track, int slot) {
        setProcessorSlotSelected(track, slot, false, false);
    }

    void setTrackSelected(const ValueTree& track, bool selected, bool deselectOthers) {
        undoManager.perform(new SelectTrackAction(track, selected, deselectOthers, tracks, connections, view, input, *this));
    }

    ValueTree getFocusedTrack() const {
        return tracks.getFocusedTrack();
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

    void navigateUp() { findItemToSelectWithUpDownDelta(-1).select(*this); }

    void navigateDown() { findItemToSelectWithUpDownDelta(1).select(*this); }

    void navigateLeft() { findItemToSelectWithLeftRightDelta(-1).select(*this); }

    void navigateRight() { findItemToSelectWithLeftRightDelta(1).select(*this); }

    bool canNavigateUp() const { return findItemToSelectWithUpDownDelta(-1).isValid(); }

    bool canNavigateDown() const { return findItemToSelectWithUpDownDelta(1).isValid(); }

    bool canNavigateLeft() const { return findItemToSelectWithLeftRightDelta(-1).isValid(); }

    bool canNavigateRight() const { return findItemToSelectWithLeftRightDelta(1).isValid(); }

    void createDefaultProject() {
        view.initializeDefault();
        createAudioIoProcessors();
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
        tracks.saveProcessorStateInformation();
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

    static const String getFilenameSuffix() { return ".smp"; }

private:
    ValueTree state;
    PluginManager &pluginManager;
    ViewState view;
    TracksState tracks;
    InputState input;
    OutputState output;
    ConnectionsState connections;

    UndoManager &undoManager;

    AudioDeviceManager& deviceManager;

    StatefulAudioProcessorContainer* statefulAudioProcessorContainer {};
    std::unique_ptr<TrackAndSlot> selectionStartTrackAndSlot {}, selectionEndTrackAndSlot {};

    bool shiftHeld { false }, push2ShiftHeld { false };

    ValueTree currentlyDraggingTrack, currentlyDraggingProcessor;
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

    void createAudioIoProcessors() {
        {
            PluginDescription &audioInputDescription = pluginManager.getAudioInputDescription();
            ValueTree inputProcessor(IDs::PROCESSOR);
            inputProcessor.setProperty(IDs::id, audioInputDescription.createIdentifierString(), nullptr);
            inputProcessor.setProperty(IDs::name, audioInputDescription.name, nullptr);
            inputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            input.getState().appendChild(inputProcessor,  &undoManager);
            onProcessorCreated(inputProcessor);
        }
        {
            PluginDescription &audioOutputDescription = pluginManager.getAudioOutputDescription();
            ValueTree outputProcessor(IDs::PROCESSOR);
            outputProcessor.setProperty(IDs::id, audioOutputDescription.createIdentifierString(), nullptr);
            outputProcessor.setProperty(IDs::name, audioOutputDescription.name, nullptr);
            outputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            output.getState().appendChild(outputProcessor, &undoManager);
            onProcessorCreated(outputProcessor);
        }
    }
    
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &deviceManager) {
            deviceManager.updateEnabledMidiInputsAndOutputs();
            syncInputDevicesWithDeviceManager();
            syncOutputDevicesWithDeviceManager();
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            input.updateWithAudioDeviceSetup(config, &undoManager);
            output.updateWithAudioDeviceSetup(config, &undoManager);
        } else if (source == &undoManager) {
            // if there is nothing to undo, there is nothing to save!
            setChangedFlag(undoManager.canUndo());
        }
    }

    // TODO refactor into InputState
    void syncInputDevicesWithDeviceManager() {
        Array<ValueTree> inputProcessorsToDelete;
        for (const auto& inputProcessor : input.getState()) {
            if (inputProcessor.hasProperty(IDs::deviceName)) {
                const String &deviceName = inputProcessor[IDs::deviceName];
                if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                    inputProcessorsToDelete.add(inputProcessor);
                }
            }
        }
        for (const auto& inputProcessor : inputProcessorsToDelete) {
            undoManager.perform(new DeleteProcessorAction(inputProcessor, tracks, connections, *this));
        }
        for (const auto& deviceName : MidiInput::getDevices()) {
            if (deviceManager.isMidiInputEnabled(deviceName) &&
                !input.getState().getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiInputProcessor(IDs::PROCESSOR);
                midiInputProcessor.setProperty(IDs::id, MidiInputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiInputProcessor.setProperty(IDs::name, MidiInputProcessor::name(), nullptr);
                midiInputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
                midiInputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                input.getState().addChild(midiInputProcessor, -1, &undoManager);
                onProcessorCreated(midiInputProcessor);
            }
        }
    }

    void syncOutputDevicesWithDeviceManager() {
        Array<ValueTree> outputProcessorsToDelete;
        for (const auto& outputProcessor : output.getState()) {
            if (outputProcessor.hasProperty(IDs::deviceName)) {
                const String &deviceName = outputProcessor[IDs::deviceName];
                if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                    outputProcessorsToDelete.add(outputProcessor);
                }
            }
        }
        for (const auto& outputProcessor : outputProcessorsToDelete) {
            undoManager.perform(new DeleteProcessorAction(outputProcessor, tracks, connections, *this));
        }
        for (const auto& deviceName : MidiOutput::getDevices()) {
            if (deviceManager.isMidiOutputEnabled(deviceName) &&
                !output.getState().getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiOutputProcessor(IDs::PROCESSOR);
                midiOutputProcessor.setProperty(IDs::id, MidiOutputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiOutputProcessor.setProperty(IDs::name, MidiOutputProcessor::name(), nullptr);
                midiOutputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
                midiOutputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                output.getState().addChild(midiOutputProcessor, -1, &undoManager);
                onProcessorCreated(midiOutputProcessor);
            }
        }
    }

    void setProcessorSlotSelected(const ValueTree& track, int slot, bool selected, bool deselectOthers=true) {
        undoManager.perform(new SelectProcessorSlotAction(track, slot, selected, deselectOthers,
                                                          tracks, connections, view, input, *this));
    }

    void selectRectangle(const ValueTree &track, int slot) {
        const auto trackIndex = tracks.indexOf(track);
        const auto selectionStartTrackIndex = tracks.indexOf(selectionStartTrackAndSlot->track);
        const auto gridPosition = trackAndSlotToGridPosition({track, slot});
        Rectangle<int> selectionRectangle(trackAndSlotToGridPosition(*selectionStartTrackAndSlot), gridPosition);
        selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

        for (int otherTrackIndex = 0; otherTrackIndex < tracks.getNumTracks(); otherTrackIndex++) {
            auto otherTrack = tracks.getTrack(otherTrackIndex);
            bool trackSelected = (slot == -1 || selectionStartTrackAndSlot->slot == -1) &&
                                 ((selectionStartTrackIndex <= otherTrackIndex && otherTrackIndex <= trackIndex) ||
                                  (trackIndex <= otherTrackIndex && otherTrackIndex <= selectionStartTrackIndex));
            setTrackSelected(otherTrack, trackSelected, false);
            if (!trackSelected) {
                for (int otherSlot = 0; otherSlot < view.getNumAvailableSlotsForTrack(otherTrack); otherSlot++) {
                    const auto otherGridPosition = trackAndSlotToGridPosition({otherTrack, otherSlot});
                    setProcessorSlotSelected(otherTrack, otherSlot, selectionRectangle.contains(otherGridPosition), false);
                }
            }
        }
    }

    void startRectangleSelection() {
        const juce::Point<int> focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto& focusedTrack = tracks.getTrack(focusedTrackAndSlot.x);
        if (focusedTrack.isValid()) {
            selectionStartTrackAndSlot = focusedTrack[IDs::selected]
                                         ? std::make_unique<TrackAndSlot>(focusedTrack)
                                         : std::make_unique<TrackAndSlot>(focusedTrack, focusedTrackAndSlot.y);
        }
    }

    void endRectangleSelection() {
        selectionStartTrackAndSlot.reset();
        selectionEndTrackAndSlot.reset();
    }

    TrackAndSlot getMostRecentSelectedTrackAndSlot() const {
        if (selectionEndTrackAndSlot != nullptr) {
            return *selectionEndTrackAndSlot;
        } else {
            juce::Point<int> focusedTrackAndSlot = view.getFocusedTrackAndSlot();
            return {tracks.getTrack(focusedTrackAndSlot.x), focusedTrackAndSlot.y};
        }
    }

    TrackAndSlot findGridItemToSelectWithDelta(int xDelta, int yDelta) const {
        const auto gridPosition = trackAndSlotToGridPosition(getMostRecentSelectedTrackAndSlot());
        return gridPositionToTrackAndSlot(gridPosition + juce::Point<int>(xDelta, yDelta));
    }

    TrackAndSlot findItemToSelectWithLeftRightDelta(int delta) const {
        if (view.isGridPaneFocused())
            return findGridItemToSelectWithDelta(delta, 0);
        else
            return findSelectionPaneItemToSelectWithLeftRightDelta(delta);
    }

    TrackAndSlot findItemToSelectWithUpDownDelta(int delta) const {
        if (view.isGridPaneFocused())
            return findGridItemToSelectWithDelta(0, delta);
        else
            return findSelectionPaneItemToFocusWithUpDownDelta(delta);
    }

    TrackAndSlot findSelectionPaneItemToFocusWithUpDownDelta(int delta) const {
        const juce::Point<int> focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree& focusedTrack = tracks.getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return {};

        // XXX broken - EXC_BAD_...
        ValueTree focusedProcessor = {};//getFocusedProcessor();
        if (focusedProcessor.isValid()) {
            if (delta > 0 && focusedTrack[IDs::selected])
                return {focusedTrack, focusedProcessor}; // re-selecting the processor will deselect the parent.

            const auto &siblingProcessorToSelect = focusedProcessor.getSibling(delta);
            if (siblingProcessorToSelect.isValid())
                return {focusedTrack, siblingProcessorToSelect};
        } else { // no focused processor - selection is on empty slot
            auto focusedProcessorSlot = focusedTrackAndSlot.y;
            for (int slot = focusedProcessorSlot;
                 (delta < 0 ? slot >= 0 : slot < view.getNumAvailableSlotsForTrack(focusedTrack));
                 slot += delta) {
                const auto &processor = tracks.getProcessorAtSlot(focusedTrack, slot);
                if (processor.isValid())
                    return {focusedTrack, processor};
            }

            return delta < 0 && !focusedTrack[IDs::selected] ? TrackAndSlot(focusedTrack) : TrackAndSlot();
        }
    }

    TrackAndSlot findSelectionPaneItemToSelectWithLeftRightDelta(int delta) const {
        const juce::Point<int> focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree& focusedTrack = tracks.getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return {};

        const auto& siblingTrackToSelect = focusedTrack.getSibling(delta);
        if (!siblingTrackToSelect.isValid())
            return {};

        if (focusedTrack[IDs::selected] || focusedTrack.getNumChildren() == 0)
            return {siblingTrackToSelect};

        auto focusedSlot = focusedTrackAndSlot.y;
        if (focusedSlot != -1) {
            const auto& processorToSelect = tracks.findProcessorNearestToSlot(siblingTrackToSelect, focusedSlot);
            return processorToSelect.isValid() ? TrackAndSlot(siblingTrackToSelect, processorToSelect) : TrackAndSlot(siblingTrackToSelect);
        }

        return {};
    }

    const TrackAndSlot gridPositionToTrackAndSlot(const juce::Point<int> gridPosition) const {
        if (gridPosition.y >= view.getNumTrackProcessorSlots())
            return {tracks.getMasterTrack(),
                    jmin(gridPosition.x + view.getMasterViewSlotOffset() - view.getGridViewTrackOffset(),
                         view.getNumMasterProcessorSlots() - 1)};
        else
            return {tracks.getTrack(jmin(gridPosition.x, tracks.getNumNonMasterTracks() - 1)),
                    jmin(gridPosition.y + view.getGridViewSlotOffset(),
                         view.getNumTrackProcessorSlots() - 1)};
    }

    juce::Point<int> trackAndSlotToGridPosition(const TrackAndSlot& trackAndSlot) const {
        if (TracksState::isMasterTrack(trackAndSlot.track))
            return {trackAndSlot.slot + view.getGridViewTrackOffset() - view.getMasterViewSlotOffset(),
                    view.getNumTrackProcessorSlots()};
        else
            return {tracks.indexOf(trackAndSlot.track), trackAndSlot.slot};
    }

    bool doDisconnectNode(const ValueTree& processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo={}) {
        return undoManager.perform(new DisconnectProcessorAction(connections, processor, connectionType, defaults,
                                                                 custom, incoming, outgoing, excludingRemovalTo));
    }

    void updateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom=false) {
        undoManager.perform(new UpdateAllDefaultConnectionsAction(makeInvalidDefaultsIntoCustom, true, tracks,
                                                                  connections, input, *this));
    }

    void resetDefaultExternalInputs() {
        undoManager.perform(new ResetDefaultExternalInputConnectionsAction(true, connections, tracks, input, *this));
    }

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom=false) {
        undoManager.perform(new UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connections, *this));
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[IDs::name] == MidiInputProcessor::name() && !deviceManager.isMidiInputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], true);
        } else if (child[IDs::name] == MidiOutputProcessor::name() && !deviceManager.isMidiOutputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiOutputEnabled(child[IDs::deviceName], true);
        }
        if (child.hasType(IDs::TRACK)) {
            mostRecentlyCreatedTrack = child;
        } else if (child.hasType(IDs::PROCESSOR))
            mostRecentlyCreatedProcessor = child;
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child[IDs::name] == MidiInputProcessor::name() && deviceManager.isMidiInputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], false);
        } else if (child[IDs::name] == MidiOutputProcessor::name() && deviceManager.isMidiOutputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiOutputEnabled(child[IDs::deviceName], false);
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::deviceName && (tree == input.getState() || tree == output.getState())) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            const String &deviceName = tree[IDs::deviceName];

            if (tree == input.getState())
                config.inputDeviceName = deviceName;
            else if (tree == output.getState())
                config.outputDeviceName = deviceName;

            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
