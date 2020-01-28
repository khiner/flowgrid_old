#include <utility>

#pragma once

#include "processors/ProcessorManager.h"
#include "Utilities.h"
#include "StatefulAudioProcessorContainer.h"
#include "state_managers/TracksStateManager.h"
#include "state_managers/ConnectionsStateManager.h"
#include "state_managers/ViewStateManager.h"

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
                project.selectProcessorSlot(track, slot, nullptr);
            if (slot == -1 || (focusedTrack != track && focusedTrack[IDs::selected]))
                project.setTrackSelected(track, true, nullptr, false, false);
        }

        ValueTree track;
        const int slot;
    };

    Project(UndoManager &undoManager, ProcessorManager& processorManager, AudioDeviceManager& deviceManager)
            : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
              processorManager(processorManager),
              tracksManager(viewManager, *this, processorManager, undoManager), connectionsManager(*this),
              undoManager(undoManager), deviceManager(deviceManager) {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        input = ValueTree(IDs::INPUT);
        state.addChild(input, -1, nullptr);
        output = ValueTree(IDs::OUTPUT);
        state.addChild(output, -1, nullptr);
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

    ValueTree& getInput() { return input; }

    ValueTree& getOutput() { return output; }

    ValueTree& getTracks() { return tracksManager.getState(); }

    ValueTree getAudioInputProcessorState() const {
        return input.getChildWithProperty(IDs::name, processorManager.getAudioInputDescription().name);
    }

    ValueTree getAudioOutputProcessorState() const {
        return output.getChildWithProperty(IDs::name, processorManager.getAudioOutputDescription().name);
    }

    ValueTree getPush2MidiInputProcessor() const {
        return input.getChildWithProperty(IDs::deviceName, push2MidiDeviceName);
    }

    bool isPush2MidiInputProcessorConnected() const {
        return connectionsManager.isNodeConnected(getNodeIdForState(getPush2MidiInputProcessor()));
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
        prepareForDefaultConnectionAction();
        ValueTree masterTrack = tracksManager.doCreateAndAddMasterTrack();
        setTrackSelected(masterTrack, true, &undoManager, true);
        previouslyFocusedTrackIndex = viewManager.getFocusedTrackIndex();
    }

    void createTrack(bool addMixer=true) {
        prepareForDefaultConnectionAction();
        ValueTree newTrack = tracksManager.doCreateAndAddTrack(&undoManager, addMixer);
        setTrackSelected(newTrack, true, &undoManager, true);
        afterDefaultConnectionAction();
    }

    // Assumes we're always creating processors to the currently focused track (which is true as of now!)
    void createProcessor(const PluginDescription& description, int slot= -1) {
        const auto& focusedTrack = tracksManager.getFocusedTrack();
        if (focusedTrack.isValid()) {
            prepareForDefaultConnectionAction();
            ValueTree processor = tracksManager.doCreateAndAddProcessor(description, focusedTrack, &undoManager, slot);
            selectProcessor(processor, &undoManager);
            afterDefaultConnectionAction();
        }
    }

    void duplicateSelectedItems() {
        prepareForDefaultConnectionAction();
        for (auto selectedItem : tracksManager.findAllSelectedItems()) {
            duplicateItem(selectedItem, &undoManager);
        }
        afterDefaultConnectionAction();
    }

    void deleteSelectedItems() {
        prepareForDefaultConnectionAction();
        for (const auto &selectedItem : tracksManager.findAllSelectedItems()) {
            deleteTrackOrProcessor(selectedItem, &undoManager);
        }
        if (viewManager.getFocusedTrackIndex() >= tracksManager.getNumTracks()) {
            setTrackSelected(tracksManager.getTrack(tracksManager.getNumTracks() - 1), true, &undoManager);
        } else {
            selectProcessorSlot(tracksManager.getFocusedTrack(), viewManager.getFocusedProcessorSlot(), &undoManager);
        }
        afterDefaultConnectionAction();
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

    void dragProcessorToPosition(const Point<int> &trackAndSlot) {
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

    bool moveProcessor(Point<int> toTrackAndSlot, UndoManager *undoManager) {
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
    void focusOnProcessorSlot(const ValueTree& track, int slot, UndoManager *undoManager) {
        if (slot == -1) {
            const ValueTree &firstProcessor = track.getChild(0);
            slot = firstProcessor.isValid() ? int(firstProcessor[IDs::processorSlot]) : 0;
        }
        viewManager.focusOnProcessorSlot(track, slot);
        resetDefaultExternalInputs(undoManager);
    }

    void selectProcessorSlot(const ValueTree& track, int slot, UndoManager *undoManager, bool deselectOthers=true) {
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track, slot);
        if (selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, slot);
        } else {
            setProcessorSlotSelected(track, slot, true, deselectOthers);
        }
        focusOnProcessorSlot(track, slot, undoManager);
    }

    void deselectProcessorSlot(const ValueTree& track, int slot) {
        setProcessorSlotSelected(track, slot, false, false);
    }

    void selectMasterTrack() {
        setTrackSelected(tracksManager.getMasterTrack(), true, nullptr, false, false);
    }

    ValueTree getFocusedTrack() const {
        return tracksManager.getFocusedTrack();
    }

    void selectProcessor(const ValueTree& processor, UndoManager *undoManager) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], undoManager, true);
    }

    void setTrackSelected(ValueTree track, bool selected, UndoManager *undoManager,
                          bool deselectOthers = true, bool allowRectangleSelect = true) {
        // take care of this track
        track.setProperty(IDs::selected, selected, undoManager);
        if (selected) {
            tracksManager.selectAllTrackSlots(track, undoManager);
            focusOnProcessorSlot(track, -1, undoManager);
        } else {
            tracksManager.deselectAllTrackSlots(track, undoManager);
        }
        // take care of other tracks
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track);
        if (allowRectangleSelect && selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, -1);
        } else {
            if (selected && deselectOthers) {
                for (const auto& otherTrack : tracksManager.getState()) {
                    if (otherTrack != track) {
                        setTrackSelected(otherTrack, false, undoManager, false, false);
                    }
                }
            }
        }
    }

    void setDefaultConnectionsAllowed(ValueTree processor, bool defaultConnectionsAllowed) {
        processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, &undoManager);
        updateDefaultConnectionsForProcessor(processor);
        resetDefaultExternalInputs(&undoManager);
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
        ValueTree track = tracksManager.doCreateAndAddTrack(nullptr);
        ValueTree processor = tracksManager.doCreateAndAddProcessor(SineBank::getPluginDescription(), track, nullptr, 0);
        tracksManager.doCreateAndAddMasterTrack();
        selectProcessor(processor, nullptr);
        updateAllDefaultConnections();
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
            input.copyPropertiesFrom(newState.getChildWithName(IDs::INPUT), nullptr);
        else
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open input device \"") + inputDeviceName + "\"", failureMessage);

        if (isDeviceWithNamePresent(outputDeviceName))
            output.copyPropertiesFrom(newState.getChildWithName(IDs::OUTPUT), nullptr);
        else
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open output device \"") + outputDeviceName + "\"", failureMessage);

        Utilities::moveAllChildren(newState.getChildWithName(IDs::INPUT), input, nullptr);
        Utilities::moveAllChildren(newState.getChildWithName(IDs::OUTPUT), output, nullptr);
        tracksManager.loadFromState(newState.getChildWithName(IDs::TRACKS));
        connectionsManager.loadFromState(newState.getChildWithName(IDs::CONNECTIONS));
        selectProcessor(tracksManager.getFocusedProcessor(), nullptr);
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

    void updateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom=false) {
        for (const auto& track : getTracks()) {
            for (const auto& processor : track) {
                updateDefaultConnectionsForProcessor(processor, makeInvalidDefaultsIntoCustom);
            }
        }
        resetDefaultExternalInputs(isCurrentlyDraggingProcessor() ? nullptr : &undoManager);
    }

    bool checkedRemoveConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool defaults, bool custom) {
        const ValueTree &connectionState = connectionsManager.getConnectionMatching(connection);
        return checkedRemoveConnection(connectionState, undoManager, defaults, custom);
    }

    bool doDisconnectNode(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager,
                          bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo={}) {
        const auto connections = connectionsManager.getConnectionsForNode(nodeId, connectionType, incoming, outgoing);
        bool anyRemoved = false;
        for (const auto &connection : connections) {
            if (excludingRemovalTo != getNodeIdForState(connection.getChildWithName(IDs::DESTINATION)) &&
                checkedRemoveConnection(connection, undoManager, defaults, custom))
                anyRemoved = true;
        }

        return anyRemoved;
    }

    bool removeConnection(const AudioProcessorGraph::Connection& c) {
        const ValueTree &connectionState = connectionsManager.getConnectionMatching(c);
        bool removed = checkedRemoveConnection(c, &undoManager, true, true);
        if (removed && connectionState.hasProperty(IDs::isCustomConnection)) {
            const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto nodeId = StatefulAudioProcessorContainer::getNodeIdForState(sourceState);
            const auto& processor = getProcessorStateForNodeId(nodeId);
            updateDefaultConnectionsForProcessor(processor);
            resetDefaultExternalInputs(&undoManager);
        }
        return removed;
    }

    bool addConnection(const AudioProcessorGraph::Connection& c) {
        return checkedAddConnection(c, false, &undoManager);
    }

    bool addDefaultConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) {
        return checkedAddConnection(c, true, undoManager);
    }

    bool addCustomConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) {
        return checkedAddConnection(c, false, undoManager);
    }

    bool removeDefaultConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) {
        return checkedRemoveConnection(c, undoManager, true, false);
    }

    // Connect the given processor to the appropriate default external device input.
    bool defaultConnect(AudioProcessorGraph::NodeID fromNodeId, AudioProcessorGraph::NodeID toNodeId, ConnectionType connectionType, UndoManager *undoManager) {
        if (fromNodeId.isValid() && toNodeId.isValid()) {
            const auto &defaultConnectionChannels = connectionsManager.getDefaultConnectionChannels(connectionType);
            bool anyAdded = false;
            for (auto channel : defaultConnectionChannels) {
                if (addDefaultConnection({{fromNodeId, channel}, {toNodeId, channel}}, undoManager))
                    anyAdded = true;
            }
            return anyAdded;
        }
        return false;
    }

    // checks for duplicate add should be done before! (not done here to avoid redundant checks)
    void addConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool isDefault=true) {
        ValueTree connectionState(IDs::CONNECTION);
        ValueTree source(IDs::SOURCE);
        source.setProperty(IDs::nodeId, int(connection.source.nodeID.uid), nullptr);
        source.setProperty(IDs::channel, connection.source.channelIndex, nullptr);
        connectionState.addChild(source, -1, nullptr);

        ValueTree destination(IDs::DESTINATION);
        destination.setProperty(IDs::nodeId, int(connection.destination.nodeID.uid), nullptr);
        destination.setProperty(IDs::channel, connection.destination.channelIndex, nullptr);
        connectionState.addChild(destination, -1, nullptr);

        if (!isDefault) {
            connectionState.setProperty(IDs::isCustomConnection, true, nullptr);
        }
        connectionsManager.addConnection(connectionState, undoManager);
        if (!isDefault) {
            const auto& processor = getProcessorStateForNodeId(getNodeIdForState(source));
            updateDefaultConnectionsForProcessor(processor);
            resetDefaultExternalInputs(undoManager);
        }
    }

    bool checkedAddConnection(const AudioProcessorGraph::Connection &c, bool isDefault, UndoManager* undoManager) {
        if (isDefault && isShiftHeld())
            return false; // no default connection stuff while shift is held
        if (connectionsManager.canConnectUi(c)) {
            if (!isDefault)
                disconnectDefaultOutgoing(c.source.nodeID, c.source.isMIDI() ? midi : audio, undoManager);
            addConnection(c, undoManager, isDefault);

            return true;
        }
        return false;
    }

    bool checkedRemoveConnection(const ValueTree& connection, UndoManager* undoManager, bool allowDefaults, bool allowCustom) {
        if (!connection.isValid() || (!connection[IDs::isCustomConnection] && isShiftHeld()))
            return false; // no default connection stuff while shift is held
        return connectionsManager.removeConnection(connection, undoManager, allowDefaults, allowCustom);
    }


    ValueTree findProcessorToFlowInto(const ValueTree &track, const ValueTree &processor, ConnectionType connectionType) {
        if (!processor.hasType(IDs::PROCESSOR) || !connectionsManager.isProcessorAProducer(processor, connectionType))
            return {};

        int siblingDelta = 0;
        ValueTree otherTrack;
        while ((otherTrack = track.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : otherTrack) {
                if (otherProcessor == processor) continue;
                bool isOtherProcessorBelow = int(otherProcessor[IDs::processorSlot]) > int(processor[IDs::processorSlot]) ||
                                             (track != otherTrack && TracksStateManager::isMasterTrack(otherTrack));
                if (!isOtherProcessorBelow) continue;
                if (connectionsManager.canProcessorDefaultConnectTo(processor, otherProcessor, connectionType) ||
                    // If a non-effect (another producer) is under this processor in the same track, and no effect processors
                    // to the right have a lower slot, block this producer's output by the other producer,
                    // effectively replacing it.
                    track == otherTrack) {
                    return otherProcessor;
                }
            }
        }

        return getAudioOutputProcessorState();
    }

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom=false) {
        UndoManager *undoManager = isCurrentlyDraggingProcessor() ? nullptr : &this->undoManager;
        for (auto connectionType : {audio, midi}) {
            AudioProcessorGraph::NodeID nodeId = StatefulAudioProcessorContainer::getNodeIdForState(processor);
            if (!processor[IDs::allowDefaultConnections]) {
                doDisconnectNode(nodeId, all, undoManager, true, false, true, true);
                return;
            }
            auto outgoingCustomConnections = connectionsManager.getConnectionsForNode(nodeId, connectionType, false, true, true, false);
            if (!outgoingCustomConnections.isEmpty()) {
                disconnectDefaultOutgoing(nodeId, connectionType, undoManager);
                return;
            }
            auto outgoingDefaultConnections = connectionsManager.getConnectionsForNode(nodeId, connectionType, false, true, false, true);
            auto processorToConnectTo = findProcessorToFlowInto(processor.getParent(), processor, connectionType);
            auto nodeIdToConnectTo = StatefulAudioProcessorContainer::getNodeIdForState(processorToConnectTo);

            const auto &defaultChannels = connectionsManager.getDefaultConnectionChannels(connectionType);
            bool anyCustomAdded = false;
            for (const auto &connection : outgoingDefaultConnections) {
                AudioProcessorGraph::NodeID nodeIdCurrentlyConnectedTo = StatefulAudioProcessorContainer::getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
                if (nodeIdCurrentlyConnectedTo != nodeIdToConnectTo) {
                    for (auto channel : defaultChannels) {
                        removeDefaultConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}}, undoManager);
                    }
                    if (makeInvalidDefaultsIntoCustom) {
                        for (auto channel : defaultChannels) {
                            if (addCustomConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}}, undoManager))
                                anyCustomAdded = true;
                        }
                    }
                }
            }
            if (!anyCustomAdded) {
                if (processor[IDs::allowDefaultConnections] && processorToConnectTo[IDs::allowDefaultConnections]) {
                    for (auto channel : defaultChannels) {
                        addDefaultConnection({{nodeId, channel}, {nodeIdToConnectTo, channel}}, undoManager);
                    }
                }
            }
        }
    }

    bool disconnectProcessor(const ValueTree& processor) {
        return disconnectNode(getNodeIdForState(processor), &undoManager);
    }

    bool disconnectProcessor(const ValueTree& processor, UndoManager *undoManager) {
        return disconnectNode(getNodeIdForState(processor), undoManager);
    }

    bool disconnectNode(AudioProcessorGraph::NodeID nodeId) {
        return disconnectNode(nodeId, &undoManager);
    }

    bool disconnectNode(AudioProcessorGraph::NodeID nodeId, UndoManager *undoManager) {
        return doDisconnectNode(nodeId, all, undoManager, true, true, true, true);
    }

    bool disconnectDefaultOutgoing(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager, AudioProcessorGraph::NodeID excludingRemovalTo={}) {
        return doDisconnectNode(nodeId, connectionType, undoManager, true, false, false, true, excludingRemovalTo);
    }

    bool disconnectCustom(const ValueTree& processor) {
        return doDisconnectNode(getNodeIdForState(processor), all, &undoManager, false, true, true, true);
    }

    // Disconnect external audio/midi inputs (unless `addDefaultConnections` is true and
    // the default connection would stay the same).
    // If `addDefaultConnections` is true, then for both audio and midi connection types:
    //   * Find the topmost effect processor (receiving audio/midi) in the focused track
    //   * Connect external device inputs to its most-upstream connected processor (including itself)
    // (Note that it is possible for the same focused track to have a default audio-input processor different
    // from its default midi-input processor.)
    void resetDefaultExternalInputs(UndoManager *undoManager, bool addDefaultConnections=true) {
        const auto audioSourceNodeId = getDefaultInputNodeIdForConnectionType(audio);
        const auto midiSourceNodeId = getDefaultInputNodeIdForConnectionType(midi);

        AudioProcessorGraph::NodeID audioDestinationNodeId, midiDestinationNodeId;
        if (addDefaultConnections) {
            audioDestinationNodeId = getNodeIdForState(findEffectProcessorToReceiveDefaultExternalInput(audio));
            midiDestinationNodeId = getNodeIdForState(findEffectProcessorToReceiveDefaultExternalInput(midi));
        }

        defaultConnect(audioSourceNodeId, audioDestinationNodeId, audio, undoManager);
        disconnectDefaultOutgoing(audioSourceNodeId, audio, undoManager, audioDestinationNodeId);

        defaultConnect(midiSourceNodeId, midiDestinationNodeId, midi, undoManager);
        disconnectDefaultOutgoing(midiSourceNodeId, midi, undoManager, midiDestinationNodeId);
    }

    ValueTree findEffectProcessorToReceiveDefaultExternalInput(ConnectionType connectionType) {
        const ValueTree &focusedTrack = tracksManager.getFocusedTrack();
        const ValueTree &topmostEffectProcessor = findTopmostEffectProcessor(focusedTrack, connectionType);
        return findMostUpstreamAvailableProcessorConnectedTo(topmostEffectProcessor, connectionType);
    }


    ValueTree findTopmostEffectProcessor(const ValueTree& track, ConnectionType connectionType) {
        for (const auto& processor : track) {
            if (connectionsManager.isProcessorAnEffect(processor, connectionType)) {
                return processor;
            }
        }
        return {};
    }

private:
    ValueTree state;
    ProcessorManager &processorManager;
    ViewStateManager viewManager;
    TracksStateManager tracksManager;
    ConnectionsStateManager connectionsManager;

    UndoManager &undoManager;
    ValueTree input, output;

    AudioDeviceManager& deviceManager;

    AudioProcessorGraph* graph {};
    StatefulAudioProcessorContainer* statefulAudioProcessorContainer {};
    std::unique_ptr<TrackAndSlot> selectionStartTrackAndSlot {}, selectionEndTrackAndSlot {};
    int previouslyFocusedTrackIndex = 0;

    bool shiftHeld { false }, push2ShiftHeld { false };

    ValueTree currentlyDraggingTrack, currentlyDraggingProcessor;
    Point<int> initialDraggingTrackAndSlot, currentlyDraggingTrackAndSlot;


    void clear() {
        input.removeAllChildren(nullptr);
        output.removeAllChildren(nullptr);
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
                disconnectProcessor(item, undoManager);
            }
            item.getParent().removeChild(item, undoManager);
        }
    }

    void duplicateItem(ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid() || !item.getParent().isValid())
            return;

        if (item.hasType(IDs::TRACK) && !tracksManager.isMasterTrack(item)) {
            auto copiedTrack = tracksManager.doCreateAndAddTrack(undoManager, false, item, true);
            for (auto processor : item) {
                tracksManager.saveProcessorStateInformationToState(processor);
                auto copiedProcessor = processor.createCopy();
                copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                tracksManager.addProcessorToTrack(copiedTrack, processor, item.indexOf(processor), undoManager);
            }
        } else if (item.hasType(IDs::PROCESSOR) && !tracksManager.isMixerChannelProcessor(item)) {
            tracksManager.saveProcessorStateInformationToState(item);
            auto track = item.getParent();
            auto copiedProcessor = item.createCopy();
            tracksManager.setProcessorSlot(track, copiedProcessor, int(item[IDs::processorSlot]) + 1, nullptr);
            copiedProcessor.removeProperty(IDs::nodeId, nullptr);
            tracksManager.addProcessorToTrack(track, copiedProcessor, item.indexOf(item), undoManager);
        }
    }

    void createAudioIoProcessors() {
        {
            PluginDescription &audioInputDescription = processorManager.getAudioInputDescription();
            ValueTree inputProcessor(IDs::PROCESSOR);
            inputProcessor.setProperty(IDs::id, audioInputDescription.createIdentifierString(), nullptr);
            inputProcessor.setProperty(IDs::name, audioInputDescription.name, nullptr);
            inputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            input.addChild(inputProcessor, -1, nullptr);
        }
        {
            PluginDescription &audioOutputDescription = processorManager.getAudioOutputDescription();
            ValueTree outputProcessor(IDs::PROCESSOR);
            outputProcessor.setProperty(IDs::id, audioOutputDescription.createIdentifierString(), nullptr);
            outputProcessor.setProperty(IDs::name, audioOutputDescription.name, nullptr);
            outputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
            output.addChild(outputProcessor, -1, nullptr);
        }
    }
    
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &deviceManager) {
            deviceManager.updateEnabledMidiInputsAndOutputs();
            syncInputDevicesWithDeviceManager();
            syncOutputDevicesWithDeviceManager();
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            if (!input.hasProperty(IDs::deviceName) || input[IDs::deviceName] != config.inputDeviceName)
                input.setProperty(IDs::deviceName, config.inputDeviceName, &undoManager);
            if (!output.hasProperty(IDs::deviceName) || output[IDs::deviceName] != config.outputDeviceName)
                output.setProperty(IDs::deviceName, config.outputDeviceName, &undoManager);
        } else if (source == &undoManager) {
            // if there is nothing to undo, there is nothing to save!
            setChangedFlag(undoManager.canUndo());
        }
    }

    void syncInputDevicesWithDeviceManager() {
        Array<ValueTree> inputChildrenToDelete;
        for (const auto& inputChild : input) {
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
                !input.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiInputProcessor(IDs::PROCESSOR);
                midiInputProcessor.setProperty(IDs::id, MidiInputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiInputProcessor.setProperty(IDs::name, MidiInputProcessor::name(), nullptr);
                midiInputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
                midiInputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                input.addChild(midiInputProcessor, -1, &undoManager);
            }
        }
    }

    void syncOutputDevicesWithDeviceManager() {
        Array<ValueTree> outputChildrenToDelete;
        for (const auto& outputChild : output) {
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
                !output.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiOutputProcessor(IDs::PROCESSOR);
                midiOutputProcessor.setProperty(IDs::id, MidiOutputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiOutputProcessor.setProperty(IDs::name, MidiOutputProcessor::name(), nullptr);
                midiOutputProcessor.setProperty(IDs::allowDefaultConnections, true, nullptr);
                midiOutputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                output.addChild(midiOutputProcessor, -1, &undoManager);
            }
        }
    }

    void setProcessorSlotSelected(ValueTree track, int slot, bool selected, bool deselectOthers=true) {
        const auto currentSlotMask = tracksManager.getSlotMask(track);
        if (deselectOthers) {
            for (const auto& anyTrack : tracksManager.getState()) { // also deselect this track!
                setTrackSelected(anyTrack, false, nullptr, false, false);
            }
        }
        auto newSlotMask = deselectOthers ? BigInteger() : currentSlotMask;
        newSlotMask.setBit(slot, selected);
        track.setProperty(IDs::selectedSlotsMask, newSlotMask.toString(2), nullptr);
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
            setTrackSelected(otherTrack, trackSelected, nullptr, false, false);
            if (!trackSelected) {
                for (int otherSlot = 0; otherSlot < viewManager.getNumAvailableSlotsForTrack(otherTrack); otherSlot++) {
                    const auto otherGridPosition = trackAndSlotToGridPosition({otherTrack, otherSlot});
                    setProcessorSlotSelected(otherTrack, otherSlot, selectionRectangle.contains(otherGridPosition), false);
                }
            }
        }
        focusOnProcessorSlot(track, slot, nullptr);
    }

    void startRectangleSelection() {
        const Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
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
            Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
            return {tracksManager.getTrack(focusedTrackAndSlot.x), focusedTrackAndSlot.y};
        }
    }

    TrackAndSlot findGridItemToSelectWithDelta(int xDelta, int yDelta) const {
        const auto gridPosition = trackAndSlotToGridPosition(getMostRecentSelectedTrackAndSlot());
        return gridPositionToTrackAndSlot(gridPosition + Point<int>(xDelta, yDelta));
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
        const Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
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
        const Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
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

    const TrackAndSlot gridPositionToTrackAndSlot(const Point<int> gridPosition) const {
        if (gridPosition.y >= viewManager.getNumTrackProcessorSlots())
            return {tracksManager.getMasterTrack(),
                    jmin(gridPosition.x + viewManager.getMasterViewSlotOffset() - viewManager.getGridViewTrackOffset(),
                         viewManager.getNumMasterProcessorSlots() - 1)};
        else
            return {tracksManager.getTrack(jmin(gridPosition.x, tracksManager.getNumNonMasterTracks() - 1)),
                    jmin(gridPosition.y + viewManager.getGridViewSlotOffset(),
                         viewManager.getNumTrackProcessorSlots() - 1)};
    }

    Point<int> trackAndSlotToGridPosition(const TrackAndSlot& trackAndSlot) const {
        if (tracksManager.isMasterTrack(trackAndSlot.track))
            return {trackAndSlot.slot + viewManager.getGridViewTrackOffset() - viewManager.getMasterViewSlotOffset(),
                    viewManager.getNumTrackProcessorSlots()};
        else
            return {tracksManager.indexOf(trackAndSlot.track), trackAndSlot.slot};
    }

    // Selection & focus events are usually not considered undoable.
    // However, when performing multi-select-aware actions (like deleting multiple selected items),
    // we want an undo/redo to restore what the selection state looked like at the time (for consistency,
    // as well as doing things like re-focusing a processor after undoing a delete)
    // This is accomplished by re-setting all selection/focus properties, passing a flag to allow
    // no-ops into the undo-manager (otherwise re-saving the current state wouldn't be undoable).
    void prepareForDefaultConnectionAction() {
        int currentlyFocusedTrackIndex = viewManager.getFocusedTrackIndex();
        if (currentlyFocusedTrackIndex != previouslyFocusedTrackIndex) {
            viewManager.focusOnTrackIndex(previouslyFocusedTrackIndex);
            resetDefaultExternalInputs(nullptr);

            viewManager.focusOnTrackIndex(currentlyFocusedTrackIndex);
            viewManager.addFocusStateToUndoStack(&undoManager);
            resetDefaultExternalInputs(&undoManager);
        }

        for (auto track : tracksManager.getState()) {
            TracksStateManager::resetVarAllowingNoopUndo(track, IDs::selected, &undoManager);
            TracksStateManager::resetVarAllowingNoopUndo(track, IDs::selectedSlotsMask, &undoManager);
        }
    }

    void afterDefaultConnectionAction() {
        for (auto track : tracksManager.getState()) {
            TracksStateManager::resetVarAllowingNoopUndo(track, IDs::selected, &undoManager);
            TracksStateManager::resetVarAllowingNoopUndo(track, IDs::selectedSlotsMask, &undoManager);
        }
        viewManager.addFocusStateToUndoStack(&undoManager);

        updateAllDefaultConnections();
        previouslyFocusedTrackIndex = viewManager.getFocusedTrackIndex();
    }

    // Find the upper-right-most processor that flows into the given processor
    // which doesn't already have incoming node connections.
    ValueTree findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType) {
        if (!processor.isValid())
            return {};

        int lowestSlot = INT_MAX;
        ValueTree upperRightMostProcessor;
        AudioProcessorGraph::NodeID processorNodeId = getNodeIdForState(processor);
        if (isAvailableForExternalInput(processorNodeId, connectionType))
            upperRightMostProcessor = processor;

        // TODO performance improvement: only iterate over connected processors
        for (int i = tracksManager.getNumTracks() - 1; i >= 0; i--) {
            const auto& track = tracksManager.getTrack(i);
            if (track.getNumChildren() == 0)
                continue;

            const auto& firstProcessor = track.getChild(0);
            auto firstProcessorNodeId = getNodeIdForState(firstProcessor);
            int slot = firstProcessor[IDs::processorSlot];
            if (slot < lowestSlot &&
                isAvailableForExternalInput(firstProcessorNodeId, connectionType) &&
                connectionsManager.areProcessorsConnected(firstProcessorNodeId, processorNodeId)) {

                lowestSlot = slot;
                upperRightMostProcessor = firstProcessor;
            }
        }

        return upperRightMostProcessor;
    }

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) {
            return getNodeIdForState(getAudioInputProcessorState());
        } else if (connectionType == midi) {
            return getNodeIdForState(getPush2MidiInputProcessor());
        } else {
            return {};
        }
    }

    bool isAvailableForExternalInput(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType) const {
        const auto& incomingConnections = connectionsManager.getConnectionsForNode(nodeId, connectionType, true, false);
        const auto defaultInputNodeId = getDefaultInputNodeIdForConnectionType(connectionType);
        for (const auto& incomingConnection : incomingConnections) {
            if (getNodeIdForState(incomingConnection.getChildWithName(IDs::SOURCE)) != defaultInputNodeId) {
                return false;
            }
        }
        return true;
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[IDs::name] == MidiInputProcessor::name() && !deviceManager.isMidiInputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], true);
        } else if (child[IDs::name] == MidiOutputProcessor::name() && !deviceManager.isMidiOutputEnabled(child[IDs::deviceName])) {
            deviceManager.setMidiOutputEnabled(child[IDs::deviceName], true);
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
        if (i == IDs::deviceName && (tree == input || tree == output)) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            const String &deviceName = tree[IDs::deviceName];

            if (tree == input)
                config.inputDeviceName = deviceName;
            else if (tree == output)
                config.outputDeviceName = deviceName;

            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
