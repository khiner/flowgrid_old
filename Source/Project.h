#pragma once

#include <unordered_map>
#include <view/push2/Push2Colours.h>
#include "ValueTreeItems.h"
#include "processors/ProcessorManager.h"
#include "Utilities.h"
#include "StatefulAudioProcessorContainer.h"

enum ConnectionType { audio, midi, all };

class Project : public FileBasedDocument, public ProjectChangeBroadcaster, public StatefulAudioProcessorContainer,
                private ChangeListener, private ValueTree::Listener {
public:
    Project(UndoManager &undoManager, ProcessorManager& processorManager, AudioDeviceManager& deviceManager)
            : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
              undoManager(undoManager), processorManager(processorManager), deviceManager(deviceManager) {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        input = ValueTree(IDs::INPUT);
        state.addChild(input, -1, nullptr);
        output = ValueTree(IDs::OUTPUT);
        state.addChild(output, -1, nullptr);
        tracks = ValueTree(IDs::TRACKS);
        tracks.setProperty(IDs::name, "Tracks", nullptr);
        state.addChild(tracks, -1, nullptr);
        connections = ValueTree(IDs::CONNECTIONS);
        state.addChild(connections, -1, nullptr);
        viewState = ValueTree(IDs::VIEW_STATE);
        state.addChild(viewState, -1, nullptr);
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

    UndoManager& getUndoManager() { return undoManager; }

    AudioDeviceManager& getDeviceManager() { return deviceManager; }

    ValueTree& getState() { return state; }

    ValueTree& getViewState() { return viewState; }

    ValueTree& getConnections() { return connections; }

    ValueTree& getInput() { return input; }

    ValueTree& getOutput() { return output; }

    ValueTree& getTracks() { return tracks; }

    int getNumTracks() const { return tracks.getNumChildren(); }

    int getNumNonMasterTracks() const {
        return getMasterTrack().isValid() ? tracks.getNumChildren() - 1 : tracks.getNumChildren();
    }

    ValueTree getTrack(int trackIndex) const { return tracks.getChild(trackIndex); }

    ValueTree getTrackWithViewIndex(int trackViewIndex) const {
        return tracks.getChild(trackViewIndex + getGridViewTrackOffset());
    }

    ValueTree getMasterTrack() const { return tracks.getChildWithProperty(IDs::isMasterTrack, true); }

    inline ValueTree findSelectedProcessorForTrack(const ValueTree &track) const {
        for (const auto& processor : track) {
            if (processor.hasType(IDs::PROCESSOR) && processor[IDs::selected])
                return processor;
        }
        return {};
    }

    inline ValueTree getSelectedProcessor() const {
        for (const auto& track : tracks) {
            const auto& selectedProcessor = findSelectedProcessorForTrack(track);
            if (selectedProcessor.isValid())
                return selectedProcessor;
        }
        return {};
    }

    inline bool isTrackSelected(const ValueTree& track) const {
        if (track[IDs::selected])
            return true;
        for (const auto& processor : track) {
            if (processor.hasType(IDs::PROCESSOR) && processor[IDs::selected])
                return true;
        }
        return false;
    }

    inline ValueTree getSelectedTrack() const {
        for (const auto& track : tracks) {
            if (isTrackSelected(track))
                return track;
        }
        return {};
    }

    ValueTree findTrackWithUuid(const String& uuid) {
        for (const auto& track : tracks) {
            if (track[IDs::uuid] == uuid)
                return track;
        }
        return {};
    }

    const ValueTree getMixerChannelProcessorForTrack(const ValueTree& track) const {
        return track.getChildWithProperty(IDs::name, MixerChannelProcessor::name());
    }

    const ValueTree getMixerChannelProcessorForSelectedTrack() const {
        return getMixerChannelProcessorForTrack(getSelectedTrack());
    }

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
        auto push2MidiInputProcessorNodeId = getNodeIdForState(getPush2MidiInputProcessor());
        for (const auto& connection : connections) {
            if (getNodeIdForState(connection.getChildWithName(IDs::SOURCE)) == push2MidiInputProcessorNodeId)
                return true;
        }

        return false;
    }

    const bool selectedTrackHasMixerChannel() const {
        return getMixerChannelProcessorForSelectedTrack().isValid();
    }

    int getNumTrackProcessorSlots() const { return viewState[IDs::numProcessorSlots]; }
    int getNumMasterProcessorSlots() const { return viewState[IDs::numMasterProcessorSlots]; }
    int getGridViewTrackOffset() const { return viewState[IDs::gridViewTrackOffset]; }
    int getGridViewSlotOffset() const { return viewState[IDs::gridViewSlotOffset]; }
    int getMasterViewSlotOffset() const { return viewState[IDs::masterViewSlotOffset]; }
    int getViewIndexForTrack(const ValueTree& track) const {
        return track.getParent().indexOf(track) - getGridViewTrackOffset();
    }

    void addProcessorRow() {
        viewState.setProperty(IDs::numProcessorSlots, int(viewState[IDs::numProcessorSlots]) + 1, &undoManager);
        for (const auto& track : tracks) {
            if (track.hasProperty(IDs::isMasterTrack))
                continue;
            auto mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
            if (mixerChannelProcessor.isValid()) {
                setProcessorSlot(track, mixerChannelProcessor, getMixerChannelSlotForTrack(track), &undoManager);
            }
        }
    }

    void addMasterProcessorSlot() {
        viewState.setProperty(IDs::numMasterProcessorSlots, int(viewState[IDs::numMasterProcessorSlots]) + 1, &undoManager);
        const auto& masterTrack = getMasterTrack();
        auto mixerChannelProcessor = getMixerChannelProcessorForTrack(masterTrack);
        if (mixerChannelProcessor.isValid()) {
            setProcessorSlot(masterTrack, mixerChannelProcessor, getMixerChannelSlotForTrack(masterTrack), &undoManager);
        }
    }

    bool isInShiftMode() const { return numShiftsHeld >= 1; }

    bool isInNoteMode() const { return viewState[IDs::controlMode] == noteControlMode; }

    bool isInSessionMode() const { return viewState[IDs::controlMode] == sessionControlMode; }

    void setShiftMode(bool shiftMode) { shiftMode ? numShiftsHeld++ : numShiftsHeld--; }
    void setNoteMode() { viewState.setProperty(IDs::controlMode, noteControlMode, nullptr); }
    void setSessionMode() { viewState.setProperty(IDs::controlMode, sessionControlMode, nullptr); }

    void setTrackWidth(int trackWidth) { this->trackWidth = trackWidth; }
    void setProcessorHeight(int processorHeight) { this->processorHeight = processorHeight; }

    int getTrackWidth() { return trackWidth; }
    int getProcessorHeight() { return processorHeight; }

    Array<ValueTree> getConnectionsForNode(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType,
                                           bool incoming=true, bool outgoing=true,
                                           bool includeCustom=true, bool includeDefault=true) const {
        Array<ValueTree> nodeConnections;
        for (const auto& connection : connections) {
            if ((connection[IDs::isCustomConnection] && !includeCustom) || (!connection[IDs::isCustomConnection] && !includeDefault))
                continue;

            const auto &endpointType = connection.getChildWithProperty(IDs::nodeId, int(nodeId));
            bool directionIsAcceptable = (incoming && endpointType.hasType(IDs::DESTINATION)) || (outgoing && endpointType.hasType(IDs::SOURCE));
            bool typeIsAcceptable = connectionType == all || (connectionType == audio && int(endpointType[IDs::channel]) != AudioProcessorGraph::midiChannelIndex) ||
                                    (connectionType == midi && int(endpointType[IDs::channel]) == AudioProcessorGraph::midiChannelIndex);

            if (directionIsAcceptable && typeIsAcceptable)
                nodeConnections.add(connection);
        }
        
        return nodeConnections;
    }

    const ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : connections) {
            auto sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto destState = connectionState.getChildWithName(IDs::DESTINATION);
            if (getNodeIdForState(sourceState) == connection.source.nodeID &&
                getNodeIdForState(destState) == connection.destination.nodeID &&
                int(sourceState[IDs::channel]) == connection.source.channelIndex &&
                int(destState[IDs::channel]) == connection.destination.channelIndex) {
                return connectionState;
            }
        }
        return {};
    }

    bool areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId) const {
        if (upstreamNodeId == downstreamNodeId)
            return true;

        Array<AudioProcessorGraph::NodeID> exploredDownstreamNodes;
        for (const auto& connection : connections) {
            if (getNodeIdForState(connection.getChildWithName(IDs::SOURCE)) == upstreamNodeId) {
                auto otherDownstreamNodeId = getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
                if (!exploredDownstreamNodes.contains(otherDownstreamNodeId)) {
                    if (otherDownstreamNodeId == downstreamNodeId)
                        return true;
                    else if (areProcessorsConnected(otherDownstreamNodeId, downstreamNodeId))
                        return true;
                    exploredDownstreamNodes.add(otherDownstreamNodeId);
                }
            }
        }
        return false;
    }
    
    bool hasIncomingConnections(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType) const {
        return !getConnectionsForNode(nodeId, connectionType, true, false).isEmpty();
    }

    // checks for duplicate add should be done before! (not done here to avoid redundant checks)
    void addConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool isDefault=true) {
        if (isDefault && isInShiftMode())
            return; // no default connection stuff while shift is held
        ValueTree connectionState(IDs::CONNECTION);
        ValueTree source(IDs::SOURCE);
        source.setProperty(IDs::nodeId, int(connection.source.nodeID), nullptr);
        source.setProperty(IDs::channel, connection.source.channelIndex, nullptr);
        connectionState.addChild(source, -1, nullptr);

        ValueTree destination(IDs::DESTINATION);
        destination.setProperty(IDs::nodeId, int(connection.destination.nodeID), nullptr);
        destination.setProperty(IDs::channel, connection.destination.channelIndex, nullptr);
        connectionState.addChild(destination, -1, nullptr);

        if (!isDefault) {
            connectionState.setProperty(IDs::isCustomConnection, true, nullptr);
        }
        connections.addChild(connectionState, -1, undoManager);
    }

    bool removeConnection(const ValueTree& connection, UndoManager* undoManager, bool allowDefaults, bool allowCustom) {
        if (!connection.isValid() || (!connection[IDs::isCustomConnection] && isInShiftMode()))
            return false; // no default connection stuff while shift is held

        bool customIsAcceptable = (allowCustom && connection.hasProperty(IDs::isCustomConnection)) ||
                                  (allowDefaults && !connection.hasProperty(IDs::isCustomConnection));
        if (customIsAcceptable) {
            connections.removeChild(connection, undoManager);
            return true;
        }
        return false;
    }

    bool removeConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool defaults, bool custom) {
        const ValueTree &connectionState = getConnectionMatching(connection);
        return removeConnection(connectionState, undoManager, defaults, custom);
    }

    // make a snapshot of all the information needed to capture AudioGraph connections and UI positions
    void makeConnectionsSnapshot() {
        connectionsSnapshot.clear();
        for (auto connection : connections) {
            connectionsSnapshot.add(connection.createCopy());
        }
        slotForNodeIdSnapshot.clear();
        for (auto track : tracks) {
            for (auto child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    slotForNodeIdSnapshot.insert(std::__1::pair<int, int>(child[IDs::nodeId], child[IDs::processorSlot]));
                }
            }
        }
    }

    void restoreConnectionsSnapshot() {
        connections.removeAllChildren(nullptr);
        for (const auto& connection : connectionsSnapshot) {
            connections.addChild(connection, -1, nullptr);
        }
        for (const auto& track : tracks) {
            for (auto child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    setProcessorSlot(track, child, slotForNodeIdSnapshot.at(int(child[IDs::nodeId])), nullptr);
                }
            }
        }
    }

    void navigateUp() { selectItemIfValid(findItemToSelectWithUpDownDelta(-1)); }

    void navigateDown() { selectItemIfValid(findItemToSelectWithUpDownDelta(1)); }

    void navigateLeft() { selectItemIfValid(findItemToSelectWithLeftRightDelta(-1)); }

    void navigateRight() { selectItemIfValid(findItemToSelectWithLeftRightDelta(1)); }

    bool canNavigateUp() const { return findItemToSelectWithUpDownDelta(-1).isValid(); }

    bool canNavigateDown() const { return findItemToSelectWithUpDownDelta(1).isValid(); }

    bool canNavigateLeft() const { return findItemToSelectWithLeftRightDelta(-1).isValid(); }

    bool canNavigateRight() const { return findItemToSelectWithLeftRightDelta(1).isValid(); }

    bool canDuplicateSelected() const {
        const auto& selectedTrack = getSelectedTrack();
        if (selectedTrack[IDs::selected] && !selectedTrack.hasProperty(IDs::isMasterTrack))
            return true;
        const auto& selectedProcessor = getSelectedProcessor();
        return selectedProcessor.isValid() && !isMixerChannelProcessor(selectedProcessor);
    }

    void deleteSelectedItems() {
        Array<ValueTree> allSelectedItems = findAllItemsWithPropertyRecursive(state, IDs::selected, true);
        for (const auto &selectedItem : allSelectedItems) {
            deleteItem(selectedItem, &undoManager);
        }
    }

    void duplicateSelectedItems() {
        Array<ValueTree> allSelectedItems = findAllItemsWithPropertyRecursive(state, IDs::selected, true);
        for (auto &selectedItem : allSelectedItems) {
            duplicateItem(selectedItem, &undoManager);
        }
    }

    void duplicateItem(ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK) && !item.hasProperty(IDs::isMasterTrack)) {
                auto copiedTrack = createAndAddTrack(undoManager, false, item, true);
                for (auto processor : item) {
                    saveProcessorStateInformationToState(processor);
                    auto copiedProcessor = processor.createCopy();
                    copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                    addProcessorToTrack(copiedTrack, copiedProcessor, item.indexOf(processor), undoManager);
                }
            } else if (item.hasType(IDs::PROCESSOR) && !isMixerChannelProcessor(item)) {
                saveProcessorStateInformationToState(item);
                auto track = item.getParent();
                auto copiedProcessor = item.createCopy();
                setProcessorSlot(track, copiedProcessor, int(item[IDs::processorSlot]) + 1, nullptr);
                copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                addProcessorToTrack(track, copiedProcessor, track.indexOf(item), undoManager);
            }
        }
    }

    void deleteItem(const ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK)) {
                while (item.getNumChildren() > 0)
                    deleteItem(item.getChild(item.getNumChildren() - 1), undoManager);
            } else if (item.hasType(IDs::PROCESSOR)) {
                sendProcessorWillBeDestroyedMessage(item);
            }
            item.getParent().removeChild(item, undoManager);
            if (item.hasType(IDs::PROCESSOR)) {
                sendProcessorHasBeenDestroyedMessage(item);
            }
        }
    }

    void createDefaultProject() {
        viewState.setProperty(IDs::controlMode, noteControlMode, nullptr);
        viewState.setProperty(IDs::numProcessorSlots, 7, nullptr);
        viewState.setProperty(IDs::numMasterProcessorSlots, 8, nullptr);
        viewState.setProperty(IDs::gridViewTrackOffset, 0, nullptr);
        viewState.setProperty(IDs::gridViewSlotOffset, 0, nullptr);
        viewState.setProperty(IDs::masterViewSlotOffset, 0, nullptr);

        createAudioIoProcessors();

        ValueTree track = createAndAddTrack(nullptr);
        createAndAddProcessor(SineBank::getPluginDescription(), track, nullptr, -1);

        createAndAddMasterTrack(nullptr, true);
    }

    ValueTree createAndAddMasterTrack(UndoManager* undoManager, bool addMixer=true) {
        if (getMasterTrack().isValid())
            return {}; // only one master track allowed!

        ValueTree masterTrack(IDs::TRACK);
        masterTrack.setProperty(IDs::isMasterTrack, true, nullptr);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        tracks.addChild(masterTrack, -1, undoManager);

        if (addMixer)
            createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack, undoManager, -1);

        return masterTrack;
    }

    ValueTree createAndAddTrack(UndoManager* undoManager, bool addMixer=true, ValueTree nextToTrack={}, bool forceImmediatelyToRight=false) {
        int numTracks = getNumNonMasterTracks();
        const auto& selectedTrack = getSelectedTrack();

        if (!nextToTrack.isValid()) {
            if (selectedTrack.isValid()) {
                // If a track is selected, insert the new track to the left of it if there's no mixer,
                // or to the right of the first track with a mixer if the new track has a mixer.
                nextToTrack = selectedTrack;

                if (addMixer && !forceImmediatelyToRight) {
                    while (nextToTrack.isValid() && !getMixerChannelProcessorForTrack(nextToTrack).isValid())
                        nextToTrack = nextToTrack.getSibling(1);
                }
            }
        }
        if (nextToTrack == getMasterTrack())
            nextToTrack = {};

        ValueTree track(IDs::TRACK);
        track.setProperty(IDs::uuid, Uuid().toString(), nullptr);
        track.setProperty(IDs::name, (nextToTrack.isValid() && !addMixer) ? makeTrackNameUnique(nextToTrack[IDs::name]) : ("Track " + String(numTracks + 1)), nullptr);
        track.setProperty(IDs::colour, (nextToTrack.isValid() && !addMixer) ? nextToTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
        tracks.addChild(track, nextToTrack.isValid() ? nextToTrack.getParent().indexOf(nextToTrack) + (addMixer || forceImmediatelyToRight ? 1 : 0): numTracks, undoManager);

        track.setProperty(IDs::selected, true, undoManager);

        if (addMixer)
            createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), track, undoManager, -1);

        return track;
    }

    ValueTree createAndAddProcessor(const PluginDescription& description, UndoManager* undoManager) {
        if (getSelectedTrack().isValid())
            return createAndAddProcessor(description, getSelectedTrack(), undoManager, -1);
        else
            return ValueTree();
    }

    ValueTree createAndAddProcessor(const PluginDescription &description, ValueTree track, UndoManager *undoManager, int slot = -1) {
        if (description.name == MixerChannelProcessor::name() && getMixerChannelProcessorForTrack(track).isValid())
            return ValueTree(); // only one mixer channel per track

        if (processorManager.isGeneratorOrInstrument(&description) &&
            processorManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            return createAndAddProcessor(description, createAndAddTrack(undoManager, false, track), undoManager, slot);
        }

        ValueTree processor(IDs::PROCESSOR);
        processor.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processor.setProperty(IDs::name, description.name, nullptr);
        processor.setProperty(IDs::allowDefaultConnections, true, nullptr);

        processor.setProperty(IDs::selected, true, nullptr);

        int insertIndex;
        if (isMixerChannelProcessor(processor)) {
            insertIndex = -1;
            slot = getMixerChannelSlotForTrack(track);
        } else if (slot == -1) {
            if (description.numInputChannels == 0) {
                insertIndex = 0;
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
                insertIndex = mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren();
                slot = insertIndex <= 0 ? 0 : int(track.getChild(insertIndex - 1)[IDs::processorSlot]) + 1;
            }
        } else {
            setProcessorSlot(track, processor, slot, nullptr);
            insertIndex = getParentIndexForProcessor(track, processor, nullptr);
        }
        setProcessorSlot(track, processor, slot, nullptr);
        addProcessorToTrack(track, processor, insertIndex, undoManager);

        return processor;
    }

    void addProcessorToTrack(ValueTree &track, const ValueTree &processor, int insertIndex, UndoManager *undoManager) {
        track.addChild(processor, insertIndex, undoManager);
        makeSlotsValid(track, undoManager);
        sendProcessorCreatedMessage(processor);
    }

    int getNumAvailableSlotsForTrack(const ValueTree &track) const {
        return track.hasProperty(IDs::isMasterTrack) ? getNumMasterProcessorSlots() : getNumTrackProcessorSlots();
    }

    int getSlotOffsetForTrack(const ValueTree& track) const {
        return track.hasProperty(IDs::isMasterTrack) ? getMasterViewSlotOffset() : getGridViewSlotOffset();
    }

    int getMixerChannelSlotForTrack(const ValueTree& track) const {
        return getNumAvailableSlotsForTrack(track) - 1;
    }

    bool isMixerChannelProcessor(const ValueTree& processor) const {
        return processor[IDs::name] == MixerChannelProcessor::name();
    }

    bool isProcessorSlotInView(const ValueTree& track, int correctedSlot) {
        bool inView = correctedSlot >= getSlotOffsetForTrack(track) &&
                      correctedSlot < getSlotOffsetForTrack(track) + NUM_VISIBLE_TRACKS;
        if (track.hasProperty(IDs::isMasterTrack))
            inView &= getGridViewSlotOffset() + NUM_VISIBLE_TRACKS > getNumTrackProcessorSlots();
        else {
            auto trackIndex = tracks.indexOf(track);
            auto trackViewOffset = getGridViewTrackOffset();
            inView &= trackIndex >= trackViewOffset && trackIndex < trackViewOffset + NUM_VISIBLE_TRACKS;
        }
        return inView;
    }

    void setProcessorSlot(const ValueTree& track, ValueTree& processor, int newSlot, UndoManager* undoManager) {
        if (!processor.isValid())
            return;

        if (newSlot >= getMixerChannelSlotForTrack(track) && !isMixerChannelProcessor(processor)) {
            if (track.hasProperty(IDs::isMasterTrack)) {
                addMasterProcessorSlot();
            } else {
                addProcessorRow();
            }
            newSlot = getMixerChannelSlotForTrack(track) - 1;
        }
        processor.setProperty(IDs::processorSlot, newSlot, undoManager);
    }

    void makeSlotsValid(const ValueTree& parent, UndoManager* undoManager) {
        std::vector<int> slots;
        for (const ValueTree& child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                slots.push_back(int(child[IDs::processorSlot]));
            }
        }
        sort(slots.begin(), slots.end());
        for (int i = 1; i < slots.size(); i++) {
            while (slots[i] <= slots[i - 1]) {
                slots[i] += 1;
            }
        }

        auto iterator = slots.begin();
        for (ValueTree child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                int newSlot = *(iterator++);
                setProcessorSlot(parent, child, newSlot, undoManager);
            }
        }
    }

    int getParentIndexForProcessor(const ValueTree &parent, const ValueTree &processorState, UndoManager* undoManager) {
        auto slot = int(processorState[IDs::processorSlot]);
        for (ValueTree otherProcessorState : parent) {
            if (processorState == otherProcessorState)
                continue;
            if (otherProcessorState.hasType(IDs::PROCESSOR)) {
                auto otherSlot = int(otherProcessorState[IDs::processorSlot]);
                if (otherSlot == slot) {
                    if (otherProcessorState.getParent() == processorState.getParent()) {
                        // moving within same parent - need to resolve the "tie" in a way that guarantees the child order changes.
                        int currentIndex = parent.indexOf(processorState);
                        int currentOtherIndex = parent.indexOf(otherProcessorState);
                        if (currentIndex < currentOtherIndex) {
                            setProcessorSlot(parent, otherProcessorState, otherSlot - 1, undoManager);
                            return currentIndex + 2;
                        } else {
                            setProcessorSlot(parent, otherProcessorState, otherSlot + 1, undoManager);
                            return currentIndex - 1;
                        }
                    } else {
                        return parent.indexOf(otherProcessorState);
                    }
                } else if (otherSlot > slot) {
                    return parent.indexOf(otherProcessorState);
                }
            }
        }

        // TODO in this and other places, we assume processors are the only type of track child.
        return parent.getNumChildren();
    }

    ValueTree findFirstSelectedItem() {
        return findFirstItemWithPropertyRecursive(state, IDs::selected, true);
    }

    void addPluginsToMenu(PopupMenu& menu, const ValueTree& track) const {
        StringArray disabledPluginIds;
        if (getMixerChannelProcessorForTrack(track).isValid()) {
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

    PluginDescription *getTypeForIdentifier(const String &identifier) const {
        return processorManager.getDescriptionForIdentifier(identifier);
    }

    //==============================================================================================================
    void newDocument() {
        clear();
        setFile({});
        
        createDefaultProject();
        undoManager.clearUndoHistory();
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

        viewState.copyPropertiesFrom(newState.getChildWithName(IDs::VIEW_STATE), nullptr);

        const String& inputDeviceName = newState.getChildWithName(IDs::INPUT)[IDs::deviceName];
        const String& outputDeviceName = newState.getChildWithName(IDs::OUTPUT)[IDs::deviceName];

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
        Utilities::moveAllChildren(newState.getChildWithName(IDs::TRACKS), tracks, nullptr);
        Utilities::moveAllChildren(newState.getChildWithName(IDs::CONNECTIONS), connections, nullptr);

        undoManager.clearUndoHistory();
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
        for (const auto& track : tracks) {
            for (auto processorState : track) {
                saveProcessorStateInformationToState(processorState);
            }
        }
        if (Utilities::saveValueTree(state, file, true))
            return Result::ok();

        return Result::fail(TRANS("Could not save the project file"));
    }

    void saveProcessorStateInformationToState(ValueTree &processorState) const {
        if (auto* processorWrapper = getProcessorWrapperForState(processorState)) {
            MemoryBlock memoryBlock;
            if (auto* processor = processorWrapper->processor) {
                processor->getStateInformation(memoryBlock);
                processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
        }
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

    const static int NUM_VISIBLE_TRACKS = 8;
    const static int NUM_VISIBLE_PROCESSOR_SLOTS = 10;

    static constexpr int TRACK_LABEL_HEIGHT = 32;

    const String sessionControlMode = "session", noteControlMode = "note";

private:
    ValueTree state;
    UndoManager &undoManager;
    ValueTree input, output, tracks, connections, viewState;

    Array<ValueTree> connectionsSnapshot;
    std::unordered_map<int, int> slotForNodeIdSnapshot;

    ProcessorManager &processorManager;
    AudioDeviceManager& deviceManager;

    AudioProcessorGraph* graph {};
    StatefulAudioProcessorContainer* statefulAudioProcessorContainer {};

    // If either keyboard shift OR Push2 shift (or any other object calling `setShiftMode(...)`) has shift down,
    // we're in shift mode. Keep an inc/dec counter so they don't squash each other.
    int numShiftsHeld { 0 };
    int trackWidth {0}, processorHeight {0};

    void clear() {
        input.removeAllChildren(nullptr);
        output.removeAllChildren(nullptr);
        while (tracks.getNumChildren() > 0)
            deleteItem(tracks.getChild(tracks.getNumChildren() - 1), nullptr);
        connections.removeAllChildren(nullptr);
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
    
    // NOTE: assumes the track hasn't been added yet!
    const String makeTrackNameUnique(const String& trackName) {
        for (auto track : tracks) {
            String otherTrackName = track[IDs::name];
            if (otherTrackName == trackName) {
                if (trackName.contains("-")) {
                    int i = trackName.getLastCharacters(trackName.length() - trackName.lastIndexOf("-") - 1).getIntValue();
                    if (i != 0) {
                        return makeTrackNameUnique(trackName.upToLastOccurrenceOf("-", true, false) + String(i + 1));
                    }
                } else {
                    return makeTrackNameUnique(trackName + "-" + String(1));
                }
            }
        }

        return trackName;
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
        for (auto inputChild : input) {
            if (inputChild.hasProperty(IDs::deviceName)) {
                const String &deviceName = inputChild[IDs::deviceName];
                if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                    inputChildrenToDelete.add(inputChild);
                }
            }
        }
        for (const auto& inputChild : inputChildrenToDelete) {
            deleteItem(inputChild, &undoManager);
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
        for (auto outputChild : output) {
            if (outputChild.hasProperty(IDs::deviceName)) {
                const String &deviceName = outputChild[IDs::deviceName];
                if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                    outputChildrenToDelete.add(outputChild);
                }
            }
        }
        for (const auto& outputChild : outputChildrenToDelete) {
            deleteItem(outputChild, &undoManager);
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

    ValueTree findItemToSelectWithLeftRightDelta(int delta) const {
        const auto& selectedTrack = getSelectedTrack();
        if (!selectedTrack.isValid())
            return {};
        const auto& selectedProcessor = findSelectedProcessorForTrack(selectedTrack);

        if (selectedTrack.hasProperty(IDs::isMasterTrack)) {
            const auto& siblingProcessorToSelect = selectedProcessor.getSibling(delta);
            if (delta > 0) {
                if (selectedTrack[IDs::selected]) {
                    return selectedProcessor; // re-selecting the processor will deselect the parent.
                }
            } else if (delta < 0) {
                if (!siblingProcessorToSelect.isValid() && !selectedTrack[IDs::selected])
                    return selectedTrack;
            }
            return siblingProcessorToSelect;
        } else {
            auto siblingTrackToSelect = selectedTrack.getSibling(delta);
            if (siblingTrackToSelect.isValid() && !siblingTrackToSelect.hasProperty(IDs::isMasterTrack)) {
                if (selectedTrack[IDs::selected] || selectedTrack.getNumChildren() == 0) {
                    return siblingTrackToSelect;
                } else {
                    if (selectedProcessor.isValid()) {
                        int selectedProcessorSlot = selectedProcessor[IDs::processorSlot];
                        const auto& processorToSelect = findProcessorNearestToSlot(siblingTrackToSelect,
                                                                                   selectedProcessorSlot);
                        if (processorToSelect.isValid())
                            return processorToSelect;
                        else
                            return siblingTrackToSelect;
                    }
                }
            }
        }

        return {};
    }

    ValueTree findItemToSelectWithUpDownDelta(int delta) const {
        const auto& selectedProcessor = getSelectedProcessor();
        if (!selectedProcessor.isValid())
            return {};
        const auto& selectedTrack = selectedProcessor.getParent();
        int selectedProcessorSlot = selectedProcessor[IDs::processorSlot];
        if (selectedTrack.hasProperty(IDs::isMasterTrack)) {
            if (delta < 0) {
                auto trackToSelect = getTrackWithViewIndex(selectedProcessorSlot - getMasterViewSlotOffset());
                if (!trackToSelect.isValid() || trackToSelect.hasProperty(IDs::isMasterTrack))
                    trackToSelect = getTrack(getNumNonMasterTracks() - 1);
                if (trackToSelect.getNumChildren() > 0)
                    return trackToSelect.getChild(trackToSelect.getNumChildren() - 1);
                else
                    return trackToSelect;
            }
        } else {
            const auto& siblingProcessorToSelect = selectedProcessor.getSibling(delta);
            if (delta > 0) {
                if (selectedTrack[IDs::selected]) {
                    return selectedProcessor; // re-selecting the processor will deselect the parent.
                } else if (!siblingProcessorToSelect.isValid()) {
                    auto masterProcessorSlotToSelect = getViewIndexForTrack(selectedTrack) + getMasterViewSlotOffset();
                    const auto& masterTrack = getMasterTrack();
                    return findProcessorNearestToSlot(masterTrack, masterProcessorSlotToSelect);
                }
            } else if (delta < 0) {
                if (!siblingProcessorToSelect.isValid() && !selectedTrack[IDs::selected])
                    return selectedTrack;
            }

            if (siblingProcessorToSelect.isValid())
                return siblingProcessorToSelect;
        }

        return {};
    }

    void selectItemIfValid(ValueTree item) const {
        if (item.isValid()) {
            if (!item[IDs::selected])
                item.setProperty(IDs::selected, true, nullptr);
            else
                item.sendPropertyChangeMessage(IDs::selected);
        }
    }

    ValueTree findProcessorNearestToSlot(const ValueTree &track, int slot) const {
        int nearestSlot = INT_MAX;
        ValueTree nearestProcessor;
        for (const auto& processor : track) {
            int otherSlot = processor[IDs::processorSlot];
            if (abs(slot - otherSlot) < abs(slot - nearestSlot)) {
                nearestSlot = otherSlot;
                nearestProcessor = processor;
            }
            if (otherSlot > slot)
                break; // processors are ordered by slot.
        }
        return nearestProcessor;
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
        } else if ((tree.hasType(IDs::TRACK) || tree.hasType(IDs::PROCESSOR)) && i == IDs::selected && tree[IDs::selected]) {
            const auto& track = tree.hasType(IDs::TRACK) ? tree : tree.getParent();
            if (!track.hasProperty(IDs::isMasterTrack)) {
                auto trackIndex = tracks.indexOf(track);
                updateViewTrackOffsetToInclude(trackIndex);
            }
            if (tree.hasType(IDs::PROCESSOR)) {
                updateViewSlotOffsetToInclude(tree[IDs::processorSlot], track.hasProperty(IDs::isMasterTrack));
            }
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}

    void updateViewTrackOffsetToInclude(int trackIndex) {
        auto viewTrackOffset = getGridViewTrackOffset();
        if (trackIndex >= viewTrackOffset + NUM_VISIBLE_TRACKS)
            viewState.setProperty(IDs::gridViewTrackOffset, trackIndex - NUM_VISIBLE_TRACKS + 1, nullptr);
        else if (trackIndex < viewTrackOffset)
            viewState.setProperty(IDs::gridViewTrackOffset, trackIndex, nullptr);
        else if (getNumNonMasterTracks() - viewTrackOffset < NUM_VISIBLE_TRACKS && getNumNonMasterTracks() >= NUM_VISIBLE_TRACKS)
            // always show last N tracks if available
            viewState.setProperty(IDs::gridViewTrackOffset, getNumNonMasterTracks() - NUM_VISIBLE_TRACKS, nullptr);
    }

    void updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack) {
        if (isMasterTrack) {
            auto viewSlotOffset = getMasterViewSlotOffset();
            if (processorSlot >= viewSlotOffset + NUM_VISIBLE_TRACKS)
                viewState.setProperty(IDs::masterViewSlotOffset, processorSlot - NUM_VISIBLE_TRACKS + 1, nullptr);
            else if (processorSlot < viewSlotOffset)
                viewState.setProperty(IDs::masterViewSlotOffset, processorSlot, nullptr);
            processorSlot = getNumTrackProcessorSlots();
        }

        auto viewSlotOffset = getGridViewSlotOffset();
        if (processorSlot >= viewSlotOffset + NUM_VISIBLE_TRACKS)
            viewState.setProperty(IDs::gridViewSlotOffset, processorSlot - NUM_VISIBLE_TRACKS + 1, nullptr);
        else if (processorSlot < viewSlotOffset)
            viewState.setProperty(IDs::gridViewSlotOffset, processorSlot, nullptr);
    }

    static ValueTree findFirstItemWithPropertyRecursive(const ValueTree &parent, const Identifier &i, const var &value) {
        for (const auto& child : parent) {
            if (child[i] == value)
                return child;
        }
        for (const auto& child : parent) {
            const ValueTree &match = findFirstItemWithPropertyRecursive(child, i, value);
            if (match.isValid())
                return match;
        }
        return {};
    };

    static Array<ValueTree> findAllItemsWithPropertyRecursive(const ValueTree &parent, const Identifier &i, const var &value) {
        Array<ValueTree> items;
        for (const auto& child : parent) {
            if (child[i] == value)
                items.add(child);
            else
                items.addArray(findAllItemsWithPropertyRecursive(child, i, value));
        }
        return items;
    };
};
