#include <utility>
#include <state_managers/InputStateManager.h>
#include <state_managers/OutputStateManager.h>
#include <actions/SelectProcessorSlotAction.h>
#include <actions/CreateTrackAction.h>
#include <actions/CreateProcessorAction.h>

#pragma once

#include "processors/ProcessorManager.h"
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

    Project(UndoManager &undoManager, ProcessorManager& processorManager, AudioDeviceManager& deviceManager)
            : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
              processorManager(processorManager),
              tracksManager(viewManager, *this, processorManager, undoManager),
              inputManager(*this, processorManager),
              outputManager(*this, processorManager),
              connectionsManager(*this, inputManager, outputManager, tracksManager),
              undoManager(undoManager), deviceManager(deviceManager) {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        state.addChild(inputManager.getState(), -1, nullptr);
        state.addChild(outputManager.getState(), -1, nullptr);
        state.addChild(tracksManager.getState(), -1, nullptr);
        state.addChild(connectionsManager.getState(), -1, nullptr);
        state.addChild(viewManager.getState(), -1, nullptr);
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
            undoManager.perform(new CreateProcessorAction(MixerChannelProcessor::getPluginDescription(), mostRecentlyCreatedTrack, -1,
                                                          tracksManager, connectionsManager, viewManager));
        undoManager.perform(new SelectTrackAction(mostRecentlyCreatedTrack, true, true, tracksManager, connectionsManager, viewManager));
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

        if (ProcessorManager::isGeneratorOrInstrument(&description) &&
            processorManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            undoManager.perform(new CreateTrackAction(false, track, false, tracksManager, connectionsManager, viewManager));
            doCreateAndAddProcessor(description, mostRecentlyCreatedTrack, slot);
        }

        undoManager.perform(new CreateProcessorAction(description, track, slot, tracksManager, connectionsManager, viewManager));
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

        tracksManager.getState().addChild(masterTrack, -1, nullptr);

        doCreateAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack);

        return masterTrack;
    }

    void duplicateSelectedItems() {
        for (auto selectedItem : tracksManager.findAllSelectedItems()) {
            duplicateItem(selectedItem);
        }
    }

    void deleteSelectedItems() {
        for (const auto &selectedItem : tracksManager.findAllSelectedItems()) {
            deleteTrackOrProcessor(selectedItem, &undoManager);
        }
        if (viewManager.getFocusedTrackIndex() >= tracksManager.getNumTracks()) {
            setTrackSelected(tracksManager.getTrack(tracksManager.getNumTracks() - 1), true, true);
        } else {
            selectProcessorSlot(tracksManager.getFocusedTrack(), viewManager.getFocusedProcessorSlot());
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
                    connectionsManager.updateAllDefaultConnections(nullptr);
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
                connectionsManager.updateAllDefaultConnections(&undoManager, true);
                setShiftHeld(true);
            } else {
                connectionsManager.updateAllDefaultConnections(&undoManager);
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

    // Focuses on whatever is in this slot (including nothing).
    // A slot of -1 means to focus on the track, which actually focuses on
    // its first processor, if present, or its first slot (which is empty).
    void focusOnProcessorSlot(const ValueTree& track, int slot) {
        undoManager.perform(new FocusAction(track, slot, tracksManager, connectionsManager, viewManager));
    }

    void selectProcessorSlot(const ValueTree& track, int slot, bool deselectOthers=true) {
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track, slot);
        if (selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, slot);
        } else {
            setProcessorSlotSelected(track, slot, true, deselectOthers);
        }
        focusOnProcessorSlot(track, slot);
    }

    void deselectProcessorSlot(const ValueTree& track, int slot) {
        setProcessorSlotSelected(track, slot, false, false);
    }

    void setTrackSelected(const ValueTree& track, bool selected, bool deselectOthers) {
        undoManager.perform(new SelectTrackAction(track, selected, deselectOthers, tracksManager, connectionsManager, viewManager));
    }

    ValueTree getFocusedTrack() const {
        return tracksManager.getFocusedTrack();
    }

    void selectProcessor(const ValueTree& processor) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], true);
    }

    bool addConnection(const AudioProcessorGraph::Connection& c) {
        return connectionsManager.checkedAddConnection(c, false, &undoManager);
    }

    bool removeConnection(const AudioProcessorGraph::Connection& c) {
        return connectionsManager.removeConnection(c, &undoManager);
    }

    bool disconnectCustom(const ValueTree& processor) {
        return connectionsManager.disconnectCustom(processor, &undoManager);
    }

    bool disconnectProcessor(const ValueTree& processor) {
        return connectionsManager.disconnectNode(getNodeIdForState(processor), &undoManager);
    }

    void setDefaultConnectionsAllowed(ValueTree processor, bool defaultConnectionsAllowed) {
        processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, &undoManager);
        connectionsManager.updateDefaultConnectionsForProcessor(processor, isCurrentlyDraggingProcessor() ? nullptr : &this->undoManager);
        connectionsManager.resetDefaultExternalInputs(&undoManager);
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
        createTrack();
        doCreateAndAddProcessor(SineBank::getPluginDescription(), mostRecentlyCreatedTrack, 0);
        doCreateAndAddMasterTrack();
        connectionsManager.updateAllDefaultConnections(&undoManager);
        undoManager.clearUndoHistory();
    }

    void addPluginsToMenu(PopupMenu& menu, const ValueTree& track) const {
        StringArray disabledPluginIds;
        if (tracksManager.getMixerChannelProcessorForTrack(track).isValid()) {
            disabledPluginIds.add(MixerChannelProcessor::getPluginDescription().fileOrIdentifier);
        }

        PopupMenu internalSubMenu;
        PopupMenu externalSubMenu;

        getUserCreatablePluginListInternal().addToMenu(internalSubMenu, processorManager.getPluginSortMethod(), disabledPluginIds);
        getPluginListExternal().addToMenu(externalSubMenu, processorManager.getPluginSortMethod(), {}, String(), getUserCreatablePluginListInternal().getNumTypes());

        menu.addSubMenu("Internal", internalSubMenu, true);
        menu.addSeparator();
        menu.addSubMenu("External", externalSubMenu, true);
    }

    KnownPluginList &getUserCreatablePluginListInternal() const {
        return processorManager.getUserCreatablePluginListInternal();
    }

    KnownPluginList &getPluginListExternal() const {
        return processorManager.getKnownPluginListExternal();
    }

    KnownPluginList::SortMethod getPluginSortMethod() const {
        return processorManager.getPluginSortMethod();
    }

    AudioPluginFormatManager& getFormatManager() const {
        return processorManager.getFormatManager();
    }

    const PluginDescription* getChosenType(const int menuId) const {
        return processorManager.getChosenType(menuId);
    }

    std::unique_ptr<PluginDescription> getTypeForIdentifier(const String &identifier) const {
        return processorManager.getDescriptionForIdentifier(identifier);
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
        recentFiles.restoreFromString(getApplicationProperties().getUserSettings()->getValue("recentProjectFiles"));

        return recentFiles.getFile(0);
    }

    void setLastDocumentOpened(const File &file) override {
        RecentlyOpenedFilesList recentFiles;
        recentFiles.restoreFromString(getApplicationProperties().getUserSettings()->getValue("recentProjectFiles"));
        recentFiles.addFile(file);
        getApplicationProperties().getUserSettings()->setValue("recentProjectFiles", recentFiles.toString());
    }

    static const String getFilenameSuffix() { return ".smp"; }

private:
    ValueTree state;
    ProcessorManager &processorManager;
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

    ValueTree mostRecentlyCreatedTrack;

    void clear() {
        inputManager.clear();
        outputManager.clear();
        tracksManager.clear();
        connectionsManager.clear();
        undoManager.clearUndoHistory();
    }

    void deleteTrackOrProcessor(const ValueTree &item, UndoManager *undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK)) {
                while (item.getNumChildren() > 0)
                    deleteTrackOrProcessor(item.getChild(item.getNumChildren() - 1), undoManager);
            } else if (item.hasType(IDs::PROCESSOR)) {
                ValueTree(item).removeProperty(IDs::processorInitialized, nullptr);
                connectionsManager.disconnectProcessor(item, undoManager);
            }
            item.getParent().removeChild(item, undoManager);
        }
    }

    void duplicateItem(ValueTree &item) {
        if (!item.isValid() || !item.getParent().isValid())
            return;

        // TODO port to actions
        if (item.hasType(IDs::TRACK) && !TracksStateManager::isMasterTrack(item)) {
            undoManager.perform(new CreateTrackAction(false, item, true, tracksManager, connectionsManager, viewManager));
            undoManager.perform(new SelectTrackAction(mostRecentlyCreatedTrack, true, true, tracksManager, connectionsManager, viewManager));
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
            PluginDescription &audioInputDescription = processorManager.getAudioInputDescription();
            ValueTree inputProcessor(IDs::PROCESSOR);
            inputProcessor.setProperty(IDs::id, audioInputDescription.createIdentifierString(), nullptr);
            inputProcessor.setProperty(IDs::name, audioInputDescription.name, nullptr);
            inputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            inputManager.getState().addChild(inputProcessor, -1, &undoManager);
        }
        {
            PluginDescription &audioOutputDescription = processorManager.getAudioOutputDescription();
            ValueTree outputProcessor(IDs::PROCESSOR);
            outputProcessor.setProperty(IDs::id, audioOutputDescription.createIdentifierString(), nullptr);
            outputProcessor.setProperty(IDs::name, audioOutputDescription.name, nullptr);
            outputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            outputManager.getState().addChild(outputProcessor, -1, &undoManager);
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
        Array<ValueTree> inputChildrenToDelete;
        for (const auto& inputChild : inputManager.getState()) {
            if (inputChild.hasProperty(IDs::deviceName)) {
                const String &deviceName = inputChild[IDs::deviceName];
                if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                    inputChildrenToDelete.add(inputChild);
                }
            }
        }
        for (const auto& inputChild : inputChildrenToDelete) {
            deleteTrackOrProcessor(inputChild, &undoManager);
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
        Array<ValueTree> outputChildrenToDelete;
        for (const auto& outputChild : outputManager.getState()) {
            if (outputChild.hasProperty(IDs::deviceName)) {
                const String &deviceName = outputChild[IDs::deviceName];
                if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                    outputChildrenToDelete.add(outputChild);
                }
            }
        }
        for (const auto& outputChild : outputChildrenToDelete) {
            deleteTrackOrProcessor(outputChild, &undoManager);
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
        undoManager.perform(new SelectProcessorSlotAction(track, slot, selected, deselectOthers, tracksManager, connectionsManager, viewManager));
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
        focusOnProcessorSlot(track, slot);
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

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[IDs::name] == MidiInputProcessor::name() && !deviceManager.isMidiInputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], true);
        } else if (child[IDs::name] == MidiOutputProcessor::name() && !deviceManager.isMidiOutputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiOutputEnabled(child[IDs::deviceName], true);
        }
        if (child.hasType(IDs::TRACK)) {
            mostRecentlyCreatedTrack = child;
        }
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
