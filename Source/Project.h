#include <utility>

#pragma once

#include <unordered_map>
#include <view/push2/Push2Colours.h>
#include "ValueTreeItems.h"
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
                project.selectProcessorSlot(track, slot);
            if (slot == -1 || (focusedTrack != track && focusedTrack[IDs::selected]))
                project.setTrackSelected(track, true);
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

    void duplicateSelectedItems() {
        const Array<ValueTree> allSelectedItems = tracksManager.findAllSelectedItems();
        for (auto selectedItem : allSelectedItems) {
            duplicateItem(selectedItem, &undoManager);
        }
        addSelectionStateToUndoStack();
    }

    void deleteSelectedItems() {
        addSelectionStateToUndoStack();
        const Array<ValueTree> allSelectedItems = tracksManager.findAllSelectedItems();
        for (const auto &selectedItem : allSelectedItems) {
            deleteTrackOrProcessor(selectedItem, &undoManager);
        }
    }

    void deleteTrackOrProcessor(const ValueTree &item, UndoManager *undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK)) {
                while (item.getNumChildren() > 0)
                    deleteTrackOrProcessor(item.getChild(item.getNumChildren() - 1), undoManager);
            } else if (item.hasType(IDs::PROCESSOR)) {
                resetDefaultExternalInputs(getDragDependentUndoManager(), false);
                ValueTree(item).removeProperty(IDs::processorInitialized, nullptr);
                connectionsManager.disconnectNode(getNodeIdForState(item), getDragDependentUndoManager());
            }
            item.getParent().removeChild(item, undoManager);
            if (item.hasType(IDs::PROCESSOR)) {
                updateAllDefaultConnections();
            }
        }
    }

    void clearTracks() {
        while (tracksManager.getNumTracks() > 0) {
            deleteTrackOrProcessor(tracksManager.getTrack(tracksManager.getNumTracks() - 1), nullptr);
        }
    }

    void duplicateItem(ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK) && !tracksManager.isMasterTrack(item)) {
                auto copiedTrack = tracksManager.doCreateAndAddTrack(undoManager, false, item, true);
                for (auto processor : item) {
                    tracksManager.saveProcessorStateInformationToState(processor);
                    auto copiedProcessor = processor.createCopy();
                    copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                    addProcessorToTrack(copiedTrack, copiedProcessor, item.indexOf(processor), undoManager);
                }
            } else if (item.hasType(IDs::PROCESSOR) && !tracksManager.isMixerChannelProcessor(item)) {
                tracksManager.saveProcessorStateInformationToState(item);
                auto track = item.getParent();
                auto copiedProcessor = item.createCopy();
                tracksManager.setProcessorSlot(track, copiedProcessor, int(item[IDs::processorSlot]) + 1, nullptr);
                copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                addProcessorToTrack(track, copiedProcessor, track.indexOf(item), undoManager);
            }
        }
    }

    ValueTree createAndAddMasterTrack() {
        ValueTree masterTrack = tracksManager.doCreateAndAddMasterTrack();
        updateAllDefaultConnections();
        addSelectionStateToUndoStack();
        return masterTrack;
    }

    ValueTree createAndAddTrack(bool addMixer=true) {
        ValueTree newTrack = tracksManager.doCreateAndAddTrack(&undoManager, addMixer);
        setTrackSelected(newTrack, true);
        updateAllDefaultConnections();
        addSelectionStateToUndoStack();
        return newTrack;
    }

    // Assumes we're always creating processors to the currently focused track (which is true as of now!)
    ValueTree createAndAddProcessor(const PluginDescription& description, int slot=-1) {
        const auto& focusedTrack = tracksManager.getFocusedTrack();
        if (focusedTrack.isValid()) {
            ValueTree newProcessor = tracksManager.doCreateAndAddProcessor(description, focusedTrack, &undoManager, slot);
            updateAllDefaultConnections();
            addSelectionStateToUndoStack();
            return newProcessor;
        } else {
            return {};
        }
    }

    void addProcessorToTrack(ValueTree &track, const ValueTree &processor, int insertIndex, UndoManager *undoManager) {
        tracksManager.addProcessorToTrack(track, processor, insertIndex, undoManager);
        selectProcessor(processor);
        updateAllDefaultConnections();
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


    UndoManager* getDragDependentUndoManager() {
        return !currentlyDraggingProcessor.isValid() ? &undoManager : nullptr;
    }

    void dragProcessorToPosition(const Point<int> &trackAndSlot) {
        if (currentlyDraggingTrackAndSlot != trackAndSlot &&
            trackAndSlot.y < tracksManager.getMixerChannelSlotForTrack(tracksManager.getTrack(trackAndSlot.x))) {
            currentlyDraggingTrackAndSlot = trackAndSlot;

            if (moveProcessor(trackAndSlot, getDragDependentUndoManager())) {
                if (currentlyDraggingTrackAndSlot == initialDraggingTrackAndSlot)
                    restoreConnectionsSnapshot();
                else
                    updateAllDefaultConnections();
            }
        }
    }

    void endDraggingProcessor() {
        currentlyDraggingTrack = {};
        if (isCurrentlyDraggingProcessor() && currentlyDraggingTrackAndSlot != initialDraggingTrackAndSlot) {
            // update the audio graph to match the current preview UI graph.
            moveProcessor(initialDraggingTrackAndSlot, nullptr);
            restoreConnectionsSnapshot();
            moveProcessor(currentlyDraggingTrackAndSlot, &undoManager);
        }
        currentlyDraggingProcessor = {};

        if (isShiftHeld()) {
            setShiftHeld(false);
            updateAllDefaultConnections(true);
            setShiftHeld(true);
        } else {
            updateAllDefaultConnections();
        }
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
    void focusOnProcessorSlot(const ValueTree& track, int slot) {
        if (slot == -1) {
            const ValueTree &firstProcessor = track.getChild(0);
            slot = firstProcessor.isValid() ? int(firstProcessor[IDs::processorSlot]) : 0;
        }
        viewManager.focusOnProcessorSlot(track, slot);
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

    void selectMasterTrack() {
        setTrackSelected(tracksManager.getMasterTrack(), true);
    }

    ValueTree getFocusedTrack() const {
        return tracksManager.getFocusedTrack();
    }

    void selectProcessor(const ValueTree& processor) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], true);
    }

    void setTrackSelected(ValueTree track, bool selected, bool deselectOthers=true, bool allowRectangleSelect=true) {
        // take care of this track
        track.setProperty(IDs::selected, selected, nullptr);
        if (selected) {
            tracksManager.selectAllTrackSlots(track);
            focusOnProcessorSlot(track, -1);
        } else {
            tracksManager.deselectAllTrackSlots(track);
        }
        // take care of other tracks
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track);
        if (allowRectangleSelect && selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, -1);
        } else {
            if (selected && deselectOthers) {
                for (const auto& otherTrack : tracksManager.getState()) {
                    if (otherTrack != track) {
                        setTrackSelected(otherTrack, false, false);
                    }
                }
            }
        }
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
        createAndAddTrack();
        ValueTree processor = createAndAddProcessor(SineBank::getPluginDescription());
        createAndAddMasterTrack();
        selectProcessor(processor);
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

    void updateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom=false) {
        for (const auto& track : getTracks()) {
            for (const auto& processor : track) {
                updateDefaultConnectionsForProcessor(processor, false, makeInvalidDefaultsIntoCustom);
            }
        }
        resetDefaultExternalInputs(getDragDependentUndoManager());
    }


    bool removeConnection(const AudioProcessorGraph::Connection& c) {
        const ValueTree &connectionState = connectionsManager.getConnectionMatching(c);
        bool removed = connectionsManager.checkedRemoveConnection(c, getDragDependentUndoManager(), true, true);
        if (removed && connectionState.hasProperty(IDs::isCustomConnection)) {
            const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto nodeId = StatefulAudioProcessorContainer::getNodeIdForState(sourceState);
            const auto& processor = getProcessorStateForNodeId(nodeId);
            updateDefaultConnectionsForProcessor(processor, true);
        }
        return removed;
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

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool updateExternalInputs, bool makeInvalidDefaultsIntoCustom=false) {
        for (auto connectionType : {audio, midi}) {
            AudioProcessorGraph::NodeID nodeId = StatefulAudioProcessorContainer::getNodeIdForState(processor);
            if (!processor[IDs::allowDefaultConnections]) {
                connectionsManager.disconnectDefaults(nodeId, getDragDependentUndoManager());
                return;
            }
            auto outgoingCustomConnections = connectionsManager.getConnectionsForNode(nodeId, connectionType, false, true, true, false);
            if (!outgoingCustomConnections.isEmpty()) {
                connectionsManager.disconnectDefaultOutgoing(nodeId, connectionType, getDragDependentUndoManager());
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
                        connectionsManager.removeDefaultConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}}, getDragDependentUndoManager());
                    }
                    if (makeInvalidDefaultsIntoCustom) {
                        for (auto channel : defaultChannels) {
                            if (connectionsManager.addCustomConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}}, getDragDependentUndoManager()))
                                anyCustomAdded = true;
                        }
                    }
                }
            }
            if (!anyCustomAdded) {
                if (processor[IDs::allowDefaultConnections] && processorToConnectTo[IDs::allowDefaultConnections]) {
                    for (auto channel : defaultChannels) {
                        connectionsManager.addDefaultConnection({{nodeId, channel}, {nodeIdToConnectTo, channel}}, getDragDependentUndoManager());
                    }
                }
            }
        }
        if (updateExternalInputs) {
            resetDefaultExternalInputs(getDragDependentUndoManager());
        }
    }

    // Disconnect external audio/midi inputs
    // If `addDefaultConnections` is true, then for both audio and midi connection types:
    //   * Find the topmost effect processor (receiving audio/midi) in the focused track
    //   * Connect external device inputs to its most-upstream connected processor (including itself)
    // (Note that it is possible for the same focused track to have a default audio-input processor different
    // from its default midi-input processor.)
    void resetDefaultExternalInputs(UndoManager* undoManager, bool addDefaultConnections=true) {
        auto audioInputNodeId = getNodeIdForState(getAudioInputProcessorState());
        connectionsManager.disconnectDefaultOutgoing(audioInputNodeId, audio, undoManager);

        for (const auto& midiInputProcessor : getInput()) {
            if (midiInputProcessor.getProperty(IDs::name) == MidiInputProcessor::name()) {
                const auto nodeId = getNodeIdForState(midiInputProcessor);
                connectionsManager.disconnectDefaultOutgoing(nodeId, midi, undoManager);
            }
        }

        if (addDefaultConnections) {
            ValueTree processorToReceiveExternalInput = findEffectProcessorToReceiveDefaultExternalInput(audio);
            if (processorToReceiveExternalInput.isValid())
                connectionsManager.defaultConnect(audioInputNodeId, getNodeIdForState(processorToReceiveExternalInput), audio, undoManager);

            for (const auto& midiInputProcessor : getInput()) {
                if (midiInputProcessor.getProperty(IDs::name) == MidiInputProcessor::name()) {
                    const auto midiInputNodeId = getNodeIdForState(midiInputProcessor);
                    processorToReceiveExternalInput = findEffectProcessorToReceiveDefaultExternalInput(midi);
                    if (processorToReceiveExternalInput.isValid())
                        connectionsManager.defaultConnect(midiInputNodeId, getNodeIdForState(processorToReceiveExternalInput), midi, undoManager);
                }
            }
        }
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

    // Find the upper-right-most processor that flows into the given processor
    // which doesn't already have incoming node connections.
    ValueTree findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType) {
        if (!processor.isValid())
            return {};

        int lowestSlot = INT_MAX;
        ValueTree upperRightMostProcessor;
        AudioProcessorGraph::NodeID processorNodeId = getNodeIdForState(processor);
        if (!connectionsManager.nodeHasIncomingConnections(processorNodeId, connectionType))
            upperRightMostProcessor = processor;

        for (int i = tracksManager.getNumTracks() - 1; i >= 0; i--) {
            const auto& track = tracksManager.getTrack(i);
            if (track.getNumChildren() == 0)
                continue;

            const auto& firstProcessor = track.getChild(0);
            auto firstProcessorNodeId = getNodeIdForState(firstProcessor);
            int slot = firstProcessor[IDs::processorSlot];
            if (slot < lowestSlot &&
                !connectionsManager.nodeHasIncomingConnections(firstProcessorNodeId, connectionType) &&
                connectionsManager.areProcessorsConnected(firstProcessorNodeId, processorNodeId)) {

                lowestSlot = slot;
                upperRightMostProcessor = firstProcessor;
            }
        }

        return upperRightMostProcessor;
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

    bool shiftHeld { false }, push2ShiftHeld { false };

    ValueTree currentlyDraggingTrack, currentlyDraggingProcessor;
    Point<int> initialDraggingTrackAndSlot;
    Point<int> currentlyDraggingTrackAndSlot;

    void clear() {
        input.removeAllChildren(nullptr);
        output.removeAllChildren(nullptr);
        clearTracks();
        connectionsManager.clear();
        undoManager.clearUndoHistory();
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
            for (auto anyTrack : tracksManager.getState()) { // also deselect this track!
                setTrackSelected(anyTrack, false, false);
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
            setTrackSelected(otherTrack, trackSelected, false, false);
            if (!trackSelected) {
                for (int otherSlot = 0; otherSlot < viewManager.getNumAvailableSlotsForTrack(otherTrack); otherSlot++) {
                    const auto otherGridPosition = trackAndSlotToGridPosition({otherTrack, otherSlot});
                    setProcessorSlotSelected(otherTrack, otherSlot, selectionRectangle.contains(otherGridPosition), false);
                }
            }
        }
        if (slot == -1) {
            viewManager.focusOnTrackIndex(trackIndex);
        }
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
    void addSelectionStateToUndoStack() {
        for (auto track : tracksManager.getState()) {
            tracksManager.resetVarAllowingNoopUndo(track, IDs::selected, &undoManager);
            tracksManager.resetVarAllowingNoopUndo(track, IDs::selectedSlotsMask, &undoManager);
        }
        viewManager.addFocusStateToUndoStack(&undoManager);
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

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}
    void valueTreeParentChanged(ValueTree &) override {}
    void valueTreeRedirected(ValueTree &) override {}
};
