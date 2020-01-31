#include <utility>
#include <state_managers/InputStateManager.h>
#include <state_managers/OutputStateManager.h>
#include <actions/SelectProcessorSlotAction.h>
#include <actions/CreateTrackAction.h>
#include <actions/CreateProcessorAction.h>
#include <actions/DeleteSelectedItemsAction.h>
#include <actions/CreateConnectionAction.h>
#include <actions/ResetDefaultExternalInputConnectionsAction.h>
#include <actions/UpdateProcessorDefaultConnectionsAction.h>
#include <actions/SetDefaultConnectionsAllowedAction.h>
#include <actions/UpdateAllDefaultConnectionsAction.h>

#pragma once

#include "PluginManager.h"
#include "Utilities.h"
#include "StatefulAudioProcessorContainer.h"
#include "state_managers/TracksStateManager.h"
#include "state_managers/ConnectionsStateManager.h"
#include "state_managers/ViewStateManager.h"
#include "actions/SelectTrackAction.h"

class Project : public FileBasedDocument, public StatefulAudioProcessorContainer,
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
              tracksManager(viewManager, *this, pluginManager, undoManager),
              inputManager(*this, pluginManager),
              outputManager(*this, pluginManager),
              connectionsManager(*this, inputManager, outputManager, tracksManager),
              undoManager(undoManager), deviceManager(deviceManager) {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        state.appendChild(inputManager.getState(), nullptr);
        state.appendChild(outputManager.getState(), nullptr);
        state.appendChild(tracksManager.getState(), nullptr);
        state.appendChild(connectionsManager.getState(), nullptr);
        state.appendChild(viewManager.getState(), nullptr);
        undoManager.addChangeListener(this);
        state.addListener(this);
    }

    ~Project() override {
        state.removeListener(this);
        deviceManager.removeChangeListener(this);
    }

    void initialise(AudioProcessorGraph& graph) {
        this->graph = &graph;
        const auto &lastOpenedProjectFile = getLastDocumentOpened();
        if (!(lastOpenedProjectFile.exists() && loadFrom(lastOpenedProjectFile, true)))
            newDocument();
        deviceManager.addChangeListener(this);
        changeListenerCallback(&deviceManager);
        undoManager.clearUndoHistory();
    }

    void undo() {
        undoManager.undo();
    }

    void redo() {
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

    TracksStateManager& getTracksManager() { return tracksManager; }

    ConnectionsStateManager& getConnectionStateManager() { return connectionsManager; }

    ViewStateManager& getViewStateManager() { return viewManager; }

    UndoManager& getUndoManager() { return undoManager; }

    AudioDeviceManager& getDeviceManager() { return deviceManager; }

    ValueTree& getState() { return state; }

    ValueTree& getViewState() { return viewManager.getState(); }

    ValueTree& getConnections() { return connectionsManager.getState(); }

    ValueTree& getTracks() { return tracksManager.getState(); }

    bool isPush2MidiInputProcessorConnected() const {
        return connectionsManager.isNodeConnected(getNodeIdForState(inputManager.getPush2MidiInputProcessor()));
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
        ValueTree masterTrack = doCreateAndAddMasterTrack();
        setTrackSelected(masterTrack, true, true);
    }

    void createTrack(bool addMixer=true) {
        undoManager.perform(new CreateTrackAction(addMixer, {}, false, tracksManager, connectionsManager, viewManager));
        if (addMixer)
            undoManager.perform(new CreateProcessorAction(MixerChannelProcessor::getPluginDescription(), mostRecentlyCreatedTrack, -1, tracksManager, inputManager, *this));
        setTrackSelected(mostRecentlyCreatedTrack, true, true);
        updateAllDefaultConnections();
    }

    // Assumes we're always creating processors to the currently focused track (which is true as of now!)
    void createProcessor(const PluginDescription& description, int slot= -1) {
        const auto& focusedTrack = tracksManager.getFocusedTrack();
        if (focusedTrack.isValid()) {
            doCreateAndAddProcessor(description, focusedTrack, slot);
        }
    }

    void doCreateAndAddProcessor(const PluginDescription &description, const ValueTree& track, int slot=-1) {
        if (description.name == MixerChannelProcessor::name() && tracksManager.getMixerChannelProcessorForTrack(track).isValid())
            return; // only one mixer channel per track

        if (PluginManager::isGeneratorOrInstrument(&description) &&
            pluginManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            undoManager.perform(new CreateTrackAction(false, track, false, tracksManager, connectionsManager, viewManager));
            doCreateAndAddProcessor(description, mostRecentlyCreatedTrack, slot);
        }

        undoManager.perform(new CreateProcessorAction(description, track, slot, tracksManager, inputManager, *this));
        selectProcessor(mostRecentlyCreatedProcessor);
        updateAllDefaultConnections();
    }

    ValueTree doCreateAndAddMasterTrack() {
        if (tracksManager.getMasterTrack().isValid())
            return {}; // only one master track allowed!

        ValueTree masterTrack(IDs::TRACK);
        masterTrack.setProperty(IDs::isMasterTrack, true, nullptr);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        masterTrack.setProperty(IDs::selected, false, nullptr);
        masterTrack.setProperty(IDs::selectedSlotsMask, BigInteger().toString(2), nullptr);

        tracksManager.getState().appendChild(masterTrack, nullptr);

        doCreateAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack);

        return masterTrack;
    }

    void deleteSelectedItems() {
        undoManager.perform(new DeleteSelectedItemsAction(tracksManager, connectionsManager));
        if (viewManager.getFocusedTrackIndex() >= tracksManager.getNumTracks()) {
            setTrackSelected(tracksManager.getTrack(tracksManager.getNumTracks() - 1), true, true);
        } else {
            setProcessorSlotSelected(getFocusedTrack(), viewManager.getFocusedProcessorSlot(), true, false);
        }
    }

    void duplicateSelectedItems() {
        for (auto selectedItem : tracksManager.findAllSelectedItems()) {
            duplicateItem(selectedItem);
        }
    }

    void beginDraggingProcessor(const ValueTree& processor) {
        if (processor[IDs::name] == MixerChannelProcessor::name())
            // mixer channel processors are special processors.
            // they could be dragged and reconnected like any old processor, but please don't :)
            return;
        currentlyDraggingProcessor = processor;
        initialDraggingTrackAndSlot = {tracksManager.indexOf(currentlyDraggingProcessor.getParent()), currentlyDraggingProcessor[IDs::processorSlot]};
        currentlyDraggingTrackAndSlot = initialDraggingTrackAndSlot;

        makeConnectionsSnapshot();
    }

    void dragProcessorToPosition(const juce::Point<int> &trackAndSlot) {
        if (currentlyDraggingTrackAndSlot != trackAndSlot &&
            trackAndSlot.y < tracksManager.getMixerChannelSlotForTrack(tracksManager.getTrack(trackAndSlot.x))) {
            currentlyDraggingTrackAndSlot = trackAndSlot;

            if (moveProcessor(trackAndSlot, nullptr)) {
                if (currentlyDraggingTrackAndSlot == initialDraggingTrackAndSlot)
                    restoreConnectionsSnapshot();
                else
                    updateAllDefaultConnections();
            }
        }
    }

    void endDraggingProcessor() {
        if (!isCurrentlyDraggingProcessor())
            return;

        currentlyDraggingTrack = {};
        if (isCurrentlyDraggingProcessor() && currentlyDraggingTrackAndSlot != initialDraggingTrackAndSlot) {
            // update the audio graph to match the current preview UI graph.
            moveProcessor(initialDraggingTrackAndSlot, nullptr);
            restoreConnectionsSnapshot();
            // TODO update to use prepare...
            moveProcessor(currentlyDraggingTrackAndSlot, &undoManager);
            currentlyDraggingProcessor = {};
            if (isShiftHeld()) {
                setShiftHeld(false);
                updateAllDefaultConnections(true);
                setShiftHeld(true);
            } else {
                updateAllDefaultConnections();
            }
        }
        currentlyDraggingProcessor = {};
    }

    bool moveProcessor(juce::Point<int> toTrackAndSlot, UndoManager *undoManager) {
        if (!currentlyDraggingProcessor.isValid())
            return false;
        auto toTrack = tracksManager.getTrack(toTrackAndSlot.x);
        const auto& fromTrack = currentlyDraggingProcessor.getParent();
        int fromSlot = currentlyDraggingProcessor[IDs::processorSlot];
        int toSlot = toTrackAndSlot.y;
        if (fromSlot == toSlot && fromTrack == toTrack)
            return false;

        tracksManager.setProcessorSlot(fromTrack, currentlyDraggingProcessor, toSlot, undoManager);

        const int insertIndex = tracksManager.getParentIndexForProcessor(toTrack, currentlyDraggingProcessor, undoManager);
        toTrack.moveChildFromParent(fromTrack, fromTrack.indexOf(currentlyDraggingProcessor), insertIndex, undoManager);

        tracksManager.makeSlotsValid(toTrack, undoManager);

        return true;
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
        undoManager.perform(new SelectTrackAction(track, selected, deselectOthers, tracksManager, connectionsManager, viewManager, inputManager, *this));
    }

    ValueTree getFocusedTrack() const {
        return tracksManager.getFocusedTrack();
    }

    void selectProcessor(const ValueTree& processor) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], true);
    }

    bool addConnection(const AudioProcessorGraph::Connection& connection) {
        ConnectionType connectionType = connection.source.isMIDI() ? midi : audio;
        const auto& sourceProcessor = getProcessorStateForNodeId(connection.source.nodeID);
        // disconnect default outgoing
        undoManager.perform(new DisconnectProcessorAction(connectionsManager, sourceProcessor, connectionType, true, false, false, true));
        if (undoManager.perform(new CreateConnectionAction(connection, false, connectionsManager, *this))) {
            resetDefaultExternalInputs();
            return true;
        }
        return false;
    }

    bool removeConnection(const AudioProcessorGraph::Connection& connection) {
        const ValueTree &connectionState = connectionsManager.getConnectionMatching(connection);
        if (!connectionState[IDs::isCustomConnection] && isShiftHeld())
            return false; // no default connection stuff while shift is held

        bool removed = undoManager.perform(new DeleteConnectionAction(connectionState, true, true, connectionsManager));
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
        return doDisconnectNode(processor, all, false, true, true, true);
    }

    bool disconnectProcessor(const ValueTree& processor) {
        doDisconnectNode(processor, all, true, true, true, true);
    }

    void setDefaultConnectionsAllowed(const ValueTree& processor, bool defaultConnectionsAllowed) {
        undoManager.perform(new SetDefaultConnectionsAllowedAction(processor, defaultConnectionsAllowed, connectionsManager));
        resetDefaultExternalInputs();
    }

    // make a snapshot of all the information needed to capture AudioGraph connections and UI positions
    void makeConnectionsSnapshot() {
        connectionsManager.makeConnectionsSnapshot();
        tracksManager.makeConnectionsSnapshot();
    }

    void restoreConnectionsSnapshot() {
        connectionsManager.restoreConnectionsSnapshot();
        tracksManager.restoreConnectionsSnapshot();
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
        viewManager.initializeDefault();
        createAudioIoProcessors();
        doCreateAndAddMasterTrack();
        createTrack();
        doCreateAndAddProcessor(SineBank::getPluginDescription(), mostRecentlyCreatedTrack, 0);
        resetDefaultExternalInputs(); // Select action only does this if the focused track changes, so we just need to do this once ourselves
        undoManager.clearUndoHistory();
    }

    void addPluginsToMenu(PopupMenu& menu, const ValueTree& track) const {
        StringArray disabledPluginIds;
        if (tracksManager.getMixerChannelProcessorForTrack(track).isValid()) {
            disabledPluginIds.add(MixerChannelProcessor::getPluginDescription().fileOrIdentifier);
        }

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

    std::unique_ptr<PluginDescription> getTypeForIdentifier(const String &identifier) const {
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
        clear();

        const ValueTree& newState = Utilities::loadValueTree(file, true);
        if (!newState.isValid() || !newState.hasType(IDs::PROJECT))
            return Result::fail(TRANS("Not a valid project file"));

        viewManager.loadFromState(newState.getChildWithName(IDs::VIEW_STATE));

        const String& inputDeviceName = newState.getChildWithName(IDs::INPUT)[IDs::deviceName];
        const String& outputDeviceName = newState.getChildWithName(IDs::OUTPUT)[IDs::deviceName];

        // TODO this should be replaced with the greyed-out IO processor behavior (keeping connections)
        static const String& failureMessage = TRANS("Could not open an Audio IO device used by this project.  "
                                                    "All connections with the missing device will be gone.  "
                                                    "If you want this project to look like it did when you saved it, "
                                                    "the best thing to do is to reconnect the missing device and "
                                                    "reload this project (without saving first!).");

        if (isDeviceWithNamePresent(inputDeviceName))
            inputManager.loadFromState(newState.getChildWithName(IDs::INPUT));
        else
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open input device \"") + inputDeviceName + "\"", failureMessage);

        if (isDeviceWithNamePresent(outputDeviceName))
            outputManager.loadFromState(newState.getChildWithName(IDs::OUTPUT));
        else
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open output device \"") + outputDeviceName + "\"", failureMessage);

        tracksManager.loadFromState(newState.getChildWithName(IDs::TRACKS));
        connectionsManager.loadFromState(newState.getChildWithName(IDs::CONNECTIONS));
        selectProcessor(tracksManager.getFocusedProcessor());
        undoManager.clearUndoHistory();
        sendChangeMessage();

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
        tracksManager.saveProcessorStateInformation();
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
    ViewStateManager viewManager;
    TracksStateManager tracksManager;
    InputStateManager inputManager;
    OutputStateManager outputManager;
    ConnectionsStateManager connectionsManager;

    UndoManager &undoManager;

    AudioDeviceManager& deviceManager;

    AudioProcessorGraph* graph {};
    StatefulAudioProcessorContainer* statefulAudioProcessorContainer {};
    std::unique_ptr<TrackAndSlot> selectionStartTrackAndSlot {}, selectionEndTrackAndSlot {};

    bool shiftHeld { false }, push2ShiftHeld { false };

    ValueTree currentlyDraggingTrack, currentlyDraggingProcessor;
    juce::Point<int> initialDraggingTrackAndSlot, currentlyDraggingTrackAndSlot;

    ValueTree mostRecentlyCreatedTrack, mostRecentlyCreatedProcessor;

    void clear() {
        inputManager.clear();
        outputManager.clear();
        tracksManager.clear();
        connectionsManager.clear();
        undoManager.clearUndoHistory();
    }

    void duplicateItem(ValueTree &item) {
        if (!item.isValid() || !item.getParent().isValid())
            return;

        // TODO port to actions
        if (item.hasType(IDs::TRACK) && !TracksStateManager::isMasterTrack(item)) {
            undoManager.perform(new CreateTrackAction(false, item, true, tracksManager, connectionsManager, viewManager));
            setTrackSelected(mostRecentlyCreatedTrack, true, true);
            for (auto processor : item) {
                tracksManager.saveProcessorStateInformationToState(processor);
                auto copiedProcessor = processor.createCopy();
                copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                // TODO maybe refactor into an AddProcessorAction, and use that as part of CreateProcessorAction
                mostRecentlyCreatedTrack.addChild(copiedProcessor, item.indexOf(processor), &undoManager);
                selectProcessor(copiedProcessor);
            }
        } else if (item.hasType(IDs::PROCESSOR) && !tracksManager.isMixerChannelProcessor(item)) {
            tracksManager.saveProcessorStateInformationToState(item);
            auto track = item.getParent();
            auto copiedProcessor = item.createCopy();
            tracksManager.setProcessorSlot(track, copiedProcessor, int(item[IDs::processorSlot]) + 1, nullptr);
            copiedProcessor.removeProperty(IDs::nodeId, nullptr);
            // TODO AddProcessorAction
            track.addChild(copiedProcessor, track.indexOf(item), &undoManager);
            selectProcessor(copiedProcessor);
        }
    }

    void createAudioIoProcessors() {
        {
            PluginDescription &audioInputDescription = pluginManager.getAudioInputDescription();
            ValueTree inputProcessor(IDs::PROCESSOR);
            inputProcessor.setProperty(IDs::id, audioInputDescription.createIdentifierString(), nullptr);
            inputProcessor.setProperty(IDs::name, audioInputDescription.name, nullptr);
            inputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            inputManager.getState().appendChild(inputProcessor,  &undoManager);
        }
        {
            PluginDescription &audioOutputDescription = pluginManager.getAudioOutputDescription();
            ValueTree outputProcessor(IDs::PROCESSOR);
            outputProcessor.setProperty(IDs::id, audioOutputDescription.createIdentifierString(), nullptr);
            outputProcessor.setProperty(IDs::name, audioOutputDescription.name, nullptr);
            outputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            outputManager.getState().appendChild(outputProcessor, &undoManager);
        }
    }
    
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &deviceManager) {
            deviceManager.updateEnabledMidiInputsAndOutputs();
            syncInputDevicesWithDeviceManager();
            syncOutputDevicesWithDeviceManager();
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            inputManager.updateWithAudioDeviceSetup(config, &undoManager);
            outputManager.updateWithAudioDeviceSetup(config, &undoManager);
        } else if (source == &undoManager) {
            // if there is nothing to undo, there is nothing to save!
            setChangedFlag(undoManager.canUndo());
        }
    }

    // TODO refactor into InputStateManager
    void syncInputDevicesWithDeviceManager() {
        Array<ValueTree> inputProcessorsToDelete;
        for (const auto& inputProcessor : inputManager.getState()) {
            if (inputProcessor.hasProperty(IDs::deviceName)) {
                const String &deviceName = inputProcessor[IDs::deviceName];
                if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                    inputProcessorsToDelete.add(inputProcessor);
                }
            }
        }
        for (const auto& inputProcessor : inputProcessorsToDelete) {
            undoManager.perform(new DeleteProcessorAction(inputProcessor, tracksManager, connectionsManager));
        }
        for (const auto& deviceName : MidiInput::getDevices()) {
            if (deviceManager.isMidiInputEnabled(deviceName) &&
                !inputManager.getState().getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiInputProcessor(IDs::PROCESSOR);
                midiInputProcessor.setProperty(IDs::id, MidiInputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiInputProcessor.setProperty(IDs::name, MidiInputProcessor::name(), nullptr);
                midiInputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
                midiInputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                inputManager.getState().addChild(midiInputProcessor, -1, &undoManager);
            }
        }
    }

    void syncOutputDevicesWithDeviceManager() {
        Array<ValueTree> outputProcessorsToDelete;
        for (const auto& outputProcessor : outputManager.getState()) {
            if (outputProcessor.hasProperty(IDs::deviceName)) {
                const String &deviceName = outputProcessor[IDs::deviceName];
                if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                    outputProcessorsToDelete.add(outputProcessor);
                }
            }
        }
        for (const auto& outputProcessor : outputProcessorsToDelete) {
            undoManager.perform(new DeleteProcessorAction(outputProcessor, tracksManager, connectionsManager));
        }
        for (const auto& deviceName : MidiOutput::getDevices()) {
            if (deviceManager.isMidiOutputEnabled(deviceName) &&
                !outputManager.getState().getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiOutputProcessor(IDs::PROCESSOR);
                midiOutputProcessor.setProperty(IDs::id, MidiOutputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiOutputProcessor.setProperty(IDs::name, MidiOutputProcessor::name(), nullptr);
                midiOutputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
                midiOutputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                outputManager.getState().addChild(midiOutputProcessor, -1, &undoManager);
            }
        }
    }

    void setProcessorSlotSelected(const ValueTree& track, int slot, bool selected, bool deselectOthers=true) {
        undoManager.perform(new SelectProcessorSlotAction(track, slot, selected, deselectOthers,
                                                          tracksManager, connectionsManager, viewManager, inputManager, *this));
    }

    void selectRectangle(const ValueTree &track, int slot) {
        const auto trackIndex = tracksManager.indexOf(track);
        const auto selectionStartTrackIndex = tracksManager.indexOf(selectionStartTrackAndSlot->track);
        const auto gridPosition = trackAndSlotToGridPosition({track, slot});
        Rectangle<int> selectionRectangle(trackAndSlotToGridPosition(*selectionStartTrackAndSlot), gridPosition);
        selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

        for (int otherTrackIndex = 0; otherTrackIndex < tracksManager.getNumTracks(); otherTrackIndex++) {
            auto otherTrack = tracksManager.getTrack(otherTrackIndex);
            bool trackSelected = (slot == -1 || selectionStartTrackAndSlot->slot == -1) &&
                                 ((selectionStartTrackIndex <= otherTrackIndex && otherTrackIndex <= trackIndex) ||
                                  (trackIndex <= otherTrackIndex && otherTrackIndex <= selectionStartTrackIndex));
            setTrackSelected(otherTrack, trackSelected, false);
            if (!trackSelected) {
                for (int otherSlot = 0; otherSlot < viewManager.getNumAvailableSlotsForTrack(otherTrack); otherSlot++) {
                    const auto otherGridPosition = trackAndSlotToGridPosition({otherTrack, otherSlot});
                    setProcessorSlotSelected(otherTrack, otherSlot, selectionRectangle.contains(otherGridPosition), false);
                }
            }
        }
    }

    void startRectangleSelection() {
        const juce::Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
        const auto& focusedTrack = tracksManager.getTrack(focusedTrackAndSlot.x);
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
            juce::Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
            return {tracksManager.getTrack(focusedTrackAndSlot.x), focusedTrackAndSlot.y};
        }
    }

    TrackAndSlot findGridItemToSelectWithDelta(int xDelta, int yDelta) const {
        const auto gridPosition = trackAndSlotToGridPosition(getMostRecentSelectedTrackAndSlot());
        return gridPositionToTrackAndSlot(gridPosition + juce::Point<int>(xDelta, yDelta));
    }

    TrackAndSlot findItemToSelectWithLeftRightDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithDelta(delta, 0);
        else
            return findSelectionPaneItemToSelectWithLeftRightDelta(delta);
    }

    TrackAndSlot findItemToSelectWithUpDownDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithDelta(0, delta);
        else
            return findSelectionPaneItemToFocusWithUpDownDelta(delta);
    }

    TrackAndSlot findSelectionPaneItemToFocusWithUpDownDelta(int delta) const {
        const juce::Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
        const ValueTree& focusedTrack = tracksManager.getTrack(focusedTrackAndSlot.x);
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
                 (delta < 0 ? slot >= 0 : slot < viewManager.getNumAvailableSlotsForTrack(focusedTrack));
                 slot += delta) {
                const auto &processor = tracksManager.findProcessorAtSlot(focusedTrack, slot);
                if (processor.isValid())
                    return {focusedTrack, processor};
            }

            return delta < 0 && !focusedTrack[IDs::selected] ? TrackAndSlot(focusedTrack) : TrackAndSlot();
        }
    }

    TrackAndSlot findSelectionPaneItemToSelectWithLeftRightDelta(int delta) const {
        const juce::Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
        const ValueTree& focusedTrack = tracksManager.getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return {};

        const auto& siblingTrackToSelect = focusedTrack.getSibling(delta);
        if (!siblingTrackToSelect.isValid())
            return {};

        if (focusedTrack[IDs::selected] || focusedTrack.getNumChildren() == 0)
            return {siblingTrackToSelect};

        auto focusedSlot = focusedTrackAndSlot.y;
        if (focusedSlot != -1) {
            const auto& processorToSelect = tracksManager.findProcessorNearestToSlot(siblingTrackToSelect, focusedSlot);
            return processorToSelect.isValid() ? TrackAndSlot(siblingTrackToSelect, processorToSelect) : TrackAndSlot(siblingTrackToSelect);
        }

        return {};
    }

    const TrackAndSlot gridPositionToTrackAndSlot(const juce::Point<int> gridPosition) const {
        if (gridPosition.y >= viewManager.getNumTrackProcessorSlots())
            return {tracksManager.getMasterTrack(),
                    jmin(gridPosition.x + viewManager.getMasterViewSlotOffset() - viewManager.getGridViewTrackOffset(),
                         viewManager.getNumMasterProcessorSlots() - 1)};
        else
            return {tracksManager.getTrack(jmin(gridPosition.x, tracksManager.getNumNonMasterTracks() - 1)),
                    jmin(gridPosition.y + viewManager.getGridViewSlotOffset(),
                         viewManager.getNumTrackProcessorSlots() - 1)};
    }

    juce::Point<int> trackAndSlotToGridPosition(const TrackAndSlot& trackAndSlot) const {
        if (tracksManager.isMasterTrack(trackAndSlot.track))
            return {trackAndSlot.slot + viewManager.getGridViewTrackOffset() - viewManager.getMasterViewSlotOffset(),
                    viewManager.getNumTrackProcessorSlots()};
        else
            return {tracksManager.indexOf(trackAndSlot.track), trackAndSlot.slot};
    }

    bool doDisconnectNode(const ValueTree& processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo={}) {
        return undoManager.perform(new DisconnectProcessorAction(connectionsManager, processor, connectionType, defaults,
                                                                 custom, incoming, outgoing, excludingRemovalTo));
    }

    void updateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom=false) {
        undoManager.perform(new UpdateAllDefaultConnectionsAction(makeInvalidDefaultsIntoCustom, connectionsManager, tracksManager, inputManager, *this));
    }

    void resetDefaultExternalInputs() {
        undoManager.perform(new ResetDefaultExternalInputConnectionsAction(true, connectionsManager, tracksManager, inputManager, *this));
    }

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom=false) {
        undoManager.perform(new UpdateProcessorDefaultConnectionsAction(processor, makeInvalidDefaultsIntoCustom, connectionsManager, *this));
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
        if (i == IDs::deviceName && (tree == inputManager.getState() || tree == outputManager.getState())) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            const String &deviceName = tree[IDs::deviceName];

            if (tree == inputManager.getState())
                config.inputDeviceName = deviceName;
            else if (tree == outputManager.getState())
                config.outputDeviceName = deviceName;

            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
