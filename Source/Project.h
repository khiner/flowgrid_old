#pragma once

#include <unordered_map>
#include <push2/Push2MidiCommunicator.h>
#include "ValueTreeItems.h"
#include "processors/ProcessorManager.h"

class Project : public ValueTreeItem,
                public ProjectChangeBroadcaster {
public:
    Project(const ValueTree &v, UndoManager &um, ProcessorManager& processorIds)
            : ValueTreeItem(v, um), processorManager(processorIds) {
        if (!state.isValid()) {
            state = createDefaultProject();
        } else {
            output = state.getChildWithName(IDs::INPUT);
            tracks = state.getChildWithName(IDs::TRACKS);
            masterTrack = tracks.getChildWithName(IDs::MASTER_TRACK);
            connections = state.getChildWithName(IDs::CONNECTIONS);
        }
        jassert (state.hasType(IDs::PROJECT));
    }

    ValueTree& getConnections() {
        return connections;
    }

    Array<ValueTree> getConnectionsForNode(AudioProcessorGraph::NodeID nodeId) {
        Array<ValueTree> nodeConnections;
        for (const auto& connection : connections) {
            if (connection.getChildWithProperty(IDs::NODE_ID, int(nodeId)).isValid()) {
                nodeConnections.add(connection);
            }
        }

        return nodeConnections;
    }

    ValueTree& getTracks() {
        return tracks;
    }

    int getNumTracks() {
        return tracks.getNumChildren();
    }

    ValueTree getTrack(int trackIndex) {
        return tracks.getChild(trackIndex);
    }

    ValueTree& getMasterTrack() {
        return masterTrack;
    }

    const ValueTree& getSelectedTrack() const {
        return selectedTrack;
    }

    ValueTree& getSelectedProcessor() {
        return selectedProcessor;
    }

    void setSelectedProcessor(ValueTree& processor) {
        getOwnerView()->clearSelectedItems();
        processor.setProperty(IDs::selected, true, nullptr);
    }

    bool hasConnections() {
        return connections.isValid() && connections.getNumChildren() > 0;
    }

    const ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) {
        for (auto connectionState : connections) {
            auto sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto destState = connectionState.getChildWithName(IDs::DESTINATION);
            if (AudioProcessorGraph::NodeID(int(sourceState[IDs::NODE_ID])) == connection.source.nodeID &&
                AudioProcessorGraph::NodeID(int(destState[IDs::NODE_ID])) == connection.destination.nodeID &&
                int(sourceState[IDs::CHANNEL]) == connection.source.channelIndex &&
                int(destState[IDs::CHANNEL]) == connection.destination.channelIndex) {
                return connectionState;
            }
        }
        return {};

    }

    // checks for duplicate add should be done before! (not done here to avoid redundant checks)
    void addConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool isDefault=true) {
        ValueTree connectionState(IDs::CONNECTION);
        ValueTree source(IDs::SOURCE);
        source.setProperty(IDs::NODE_ID, int(connection.source.nodeID), nullptr);
        source.setProperty(IDs::CHANNEL, connection.source.channelIndex, nullptr);
        connectionState.addChild(source, -1, nullptr);

        ValueTree destination(IDs::DESTINATION);
        destination.setProperty(IDs::NODE_ID, int(connection.destination.nodeID), nullptr);
        destination.setProperty(IDs::CHANNEL, connection.destination.channelIndex, nullptr);
        connectionState.addChild(destination, -1, nullptr);

        if (!isDefault) {
            connectionState.setProperty(IDs::CUSTOM_CONNECTION, true, nullptr);
        }
        connections.addChild(connectionState, -1, undoManager);
    }

    bool removeConnection(const ValueTree& connection, UndoManager* undoManager, bool defaultOnly) {
        if (connection.isValid() && (!defaultOnly || !connection.hasProperty(IDs::CUSTOM_CONNECTION))) {
            connections.removeChild(connection, undoManager);
            return true;
        }
        return false;
    }

    bool removeConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool defaultOnly=true) {
        const ValueTree &connectionState = getConnectionMatching(connection);
        return removeConnection(connectionState, undoManager, defaultOnly);
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
                    slotForNodeIdSnapshot.insert(std::__1::pair<int, int>(child[IDs::NODE_ID], child[IDs::PROCESSOR_SLOT]));
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
                    child.setProperty(IDs::PROCESSOR_SLOT, slotForNodeIdSnapshot.at(int(child[IDs::NODE_ID])), nullptr);
                }
            }
        }
    }

    void moveSelectionUp() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::upKey));
    }

    void moveSelectionDown() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::downKey));
    }

    void moveSelectionLeft() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::leftKey));
    }

    void moveSelectionRight() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::rightKey));
    }

    void deleteSelectedItems() {
        auto selectedItems(Helpers::getSelectedAndDeletableTreeViewItems<ValueTreeItem>(*getOwnerView()));

        for (int i = selectedItems.size(); --i >= 0;) {
            ValueTree &v = *selectedItems.getUnchecked(i);

            if (v.getParent().isValid()) {
                if (v.hasType(IDs::PROCESSOR)) {
                    sendProcessorWillBeDestroyedMessage(v);
                }
                v.getParent().removeChild(v, &undoManager);
                if (v.hasType(IDs::PROCESSOR)) {
                    sendProcessorHasBeenDestroyedMessage(v);
                }
            }
        }
    }

    ValueTree createDefaultProject() {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        Helpers::createUuidProperty(state);

        output = ValueTree(IDs::INPUT);

        PluginDescription *description = processorManager.getAudioOutputDescription();
        ValueTree inputProcessor(IDs::PROCESSOR);
        Helpers::createUuidProperty(inputProcessor);
        inputProcessor.setProperty(IDs::id, description->createIdentifierString(), nullptr);
        inputProcessor.setProperty(IDs::name, description->name, nullptr);
        output.addChild(inputProcessor, -1, nullptr);
        state.addChild(output, -1, nullptr);

        tracks = ValueTree(IDs::TRACKS);
        Helpers::createUuidProperty(tracks);
        tracks.setProperty(IDs::name, "Tracks", nullptr);

        masterTrack = ValueTree(IDs::MASTER_TRACK);
        Helpers::createUuidProperty(masterTrack);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        ValueTree masterMixer = createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack, -1, false);
        masterMixer.setProperty(IDs::selected, true, nullptr);
        tracks.addChild(masterTrack, -1, nullptr);

        for (int tn = 0; tn < 1; ++tn) {
            ValueTree track = createAndAddTrack(false);
            createAndAddProcessor(SineBank::getPluginDescription(), track, false);

//            for (int cn = 0; cn < 3; ++cn) {
//                ValueTree c(IDs::CLIP);
//                Helpers::createUuidProperty(c);
//                c.setProperty(IDs::name, trackName + ", Clip " + String(cn + 1), nullptr);
//                c.setProperty(IDs::start, cn, nullptr);
//                c.setProperty(IDs::length, 1.0, nullptr);
//                c.setProperty(IDs::selected, false, nullptr);
//
//                track.addChild(c, -1, nullptr);
//            }
        }

        state.addChild(tracks, -1, nullptr);

        connections = ValueTree(IDs::CONNECTIONS);
        state.addChild(connections, -1, nullptr);

        return state;
    }

    ValueTree createAndAddTrack(bool undoable=true, bool addMixer=true, int insertIndex=-1, const String& trackName="", const String& colour="") {
        int numTracks = getNumTracks() - 1; // minus 1 because of master track

        ValueTree track(IDs::TRACK);
        Helpers::createUuidProperty(track);
        track.setProperty(IDs::name, trackName.isEmpty() ? "Track " + String(numTracks + 1) : makeTrackNameUnique(trackName), nullptr);
        track.setProperty(IDs::colour, !colour.isEmpty() ? colour : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
        const ValueTree &masterTrack = tracks.getChildWithName(IDs::MASTER_TRACK);
        if (insertIndex == -1) {
            // Insert new tracks _before_ the master track, or at the end if there isn't one.
            insertIndex = masterTrack.isValid() ? tracks.indexOf(masterTrack) : -1;
        }

        tracks.addChild(track, insertIndex, undoable ? &undoManager : nullptr);

        if (addMixer)
            createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), track, -1, undoable);

        return track;
    }

    ValueTree createAndAddProcessor(const PluginDescription& description, bool undoable=true) {
        const ValueTree& selectedTrack = getSelectedTrack();
        if (selectedTrack.isValid()) {
            return createAndAddProcessor(description, selectedTrack, -1, undoable);
        } else {
            return ValueTree();
        }
    }

    ValueTree createAndAddProcessor(const PluginDescription &description, ValueTree track, int slot=-1, bool undoable=true) {
        if (selectedTrackHasMixerChannel() && description.name == MixerChannelProcessor::getIdentifier())
            return ValueTree();

        if (processorManager.isGeneratorOrInstrument(&description) &&
            processorManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            return createAndAddProcessor(description, createAndAddTrack(undoable, false, track.getParent().indexOf(track), track.getProperty(IDs::name), track.getProperty(IDs::colour)), slot, undoable);
        }

        ValueTree processor(IDs::PROCESSOR);
        Helpers::createUuidProperty(processor);
        processor.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processor.setProperty(IDs::name, description.name, nullptr);

        // TODO can simplify
        int insertIndex;
        if (description.name == MixerChannelProcessor::getIdentifier()) {
            insertIndex = -1;
            slot = NUM_AVAILABLE_PROCESSOR_SLOTS - 1;
            processor.setProperty(IDs::PROCESSOR_SLOT, slot, nullptr);
        } else if (slot == -1) {
            // Insert new processors _right before_ the first g&b processor, or at the end if there isn't one.
            insertIndex = getMaxProcessorInsertIndex(track);
            slot = 0;
            if (track.getNumChildren() > 1) {
                slot = int(track.getChild(track.getNumChildren() - 2)[IDs::PROCESSOR_SLOT]) + 1;
            }
            processor.setProperty(IDs::PROCESSOR_SLOT, slot, nullptr);
        } else {
            processor.setProperty(IDs::PROCESSOR_SLOT, slot, nullptr);
            insertIndex = getParentIndexForProcessor(track, processor, nullptr);
        }

        track.addChild(processor, insertIndex, undoable ? &undoManager : nullptr);
        sendProcessorCreatedMessage(processor);

        return processor;
    }

    const ValueTree getMixerChannelProcessorForTrack(const ValueTree& track) const {
        return track.getChildWithProperty(IDs::name, MixerChannelProcessor::getIdentifier());
    }

    const ValueTree getMixerChannelProcessorForSelectedTrack() const {
        return getMixerChannelProcessorForTrack(getSelectedTrack());
    }

    const ValueTree getAudioOutputProcessorState() const {
        return output.getChildWithName(IDs::PROCESSOR);
    }
    AudioProcessorGraph::NodeID getAudioOutputNodeId() const {
        return AudioProcessorGraph::NodeID(int(getAudioOutputProcessorState()[IDs::NODE_ID]));
    }

    const bool selectedTrackHasMixerChannel() const {
        return getMixerChannelProcessorForSelectedTrack().isValid();
    }

    int getMaxProcessorInsertIndex(const ValueTree& track) {
        const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
        return mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren() - 1;
    }

    int getMaxAvailableProcessorSlot(const ValueTree& track) {
        const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
        return mixerChannelProcessor.isValid() ? int(mixerChannelProcessor[IDs::PROCESSOR_SLOT]) - 1 : Project::NUM_AVAILABLE_PROCESSOR_SLOTS - 1;
    }

    void makeSlotsValid(const ValueTree& parent, UndoManager *undoManager) {
        std::__1::vector<int> slots;
        for (const ValueTree& child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                slots.push_back(int(child[IDs::PROCESSOR_SLOT]));
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
                child.setProperty(IDs::PROCESSOR_SLOT, *(iterator++), undoManager);
            }
        }
    }

    int getParentIndexForProcessor(const ValueTree &parent, const ValueTree &processorState, UndoManager* undoManager) {
        auto slot = int(processorState[IDs::PROCESSOR_SLOT]);
        for (ValueTree otherProcessorState : parent) {
            if (processorState == otherProcessorState)
                continue;
            if (otherProcessorState.hasType(IDs::PROCESSOR)) {
                auto otherSlot = int(otherProcessorState[IDs::PROCESSOR_SLOT]);
                if (otherSlot == slot) {
                    if (otherProcessorState.getParent() == processorState.getParent()) {
                        // moving within same parent - need to resolve the "tie" in a way that guarantees the child order changes.
                        int currentIndex = parent.indexOf(processorState);
                        int currentOtherIndex = parent.indexOf(otherProcessorState);
                        if (currentIndex < currentOtherIndex) {
                            otherProcessorState.setProperty(IDs::PROCESSOR_SLOT, otherSlot - 1, undoManager);
                            return currentIndex + 2;
                        } else {
                            otherProcessorState.setProperty(IDs::PROCESSOR_SLOT, otherSlot + 1, undoManager);
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

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return false;
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &, int insertIndex) override {}

    bool canBeSelected() const override {
        return false;
    }

    void sendItemSelectedMessage(ValueTree item) override {
        if (item.hasType(IDs::TRACK)) {
            selectedTrack = item;
        } else if (item.getParent().hasType(IDs::TRACK)) {
            selectedTrack = item.getParent();
        } else {
            selectedTrack = ValueTree();
        }
        if (item.hasType(IDs::PROCESSOR)) {
            selectedProcessor = item;
        } else {
            selectedProcessor = ValueTree();
        }

        ProjectChangeBroadcaster::sendItemSelectedMessage(item);
    }

    void sendItemRemovedMessage(ValueTree item) override {
        if (item == selectedTrack) {
            selectedTrack = ValueTree();
        }
        if (item == selectedProcessor) {
            selectedProcessor = ValueTree();
        }
        ProjectChangeBroadcaster::sendItemSelectedMessage(item);
    }

    void addPluginsToMenu(PopupMenu& menu, const ValueTree& track) const {
        StringArray disabledPluginIds;
        if (getMixerChannelProcessorForTrack(track).isValid()) {
            disabledPluginIds.add(MixerChannelProcessor::getPluginDescription().fileOrIdentifier);
        }

        PopupMenu internalSubMenu;
        PopupMenu externalSubMenu;

        processorManager.getKnownPluginListInternal().addToMenu(internalSubMenu, processorManager.getPluginSortMethod(), disabledPluginIds);
        processorManager.getKnownPluginListExternal().addToMenu(externalSubMenu, processorManager.getPluginSortMethod(), {}, String(), processorManager.getKnownPluginListInternal().getNumTypes());

        menu.addSubMenu("Internal", internalSubMenu, true);
        menu.addSeparator();
        menu.addSubMenu("External", externalSubMenu, true);
    }

    KnownPluginList &getKnownPluginListInternal() {
        return processorManager.getKnownPluginListInternal();
    }

    KnownPluginList &getKnownPluginListExternal() {
        return processorManager.getKnownPluginListExternal();
    }

    KnownPluginList::SortMethod getPluginSortMethod() {
        return processorManager.getPluginSortMethod();
    }

    AudioPluginFormatManager& getFormatManager() {
        return processorManager.getFormatManager();
    }

    const PluginDescription* getChosenType(const int menuId) const {
        return processorManager.getChosenType(menuId);
    }

    PluginDescription *getTypeForIdentifier(const String &identifier) {
        return processorManager.getDescriptionForIdentifier(identifier);
    }

    const static int NUM_VISIBLE_TRACKS = 8;
    const static int NUM_VISIBLE_PROCESSOR_SLOTS = 9;
    // last row is reserved for audio output. second-to-last is for horizontal master track.
    const static int NUM_AVAILABLE_PROCESSOR_SLOTS = NUM_VISIBLE_PROCESSOR_SLOTS - 2;
private:
    ValueTree output;
    ValueTree tracks;
    ValueTree masterTrack;
    ValueTree selectedTrack;
    ValueTree selectedProcessor;
    ValueTree connections;

    Array<ValueTree> connectionsSnapshot;
    std::unordered_map<int, int> slotForNodeIdSnapshot;

    ProcessorManager &processorManager;

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
};
