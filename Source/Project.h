#pragma once

#include <unordered_map>
#include <push2/Push2MidiCommunicator.h>
#include "ValueTreeItems.h"
#include "processors/ProcessorManager.h"

class Project : public ProjectChangeBroadcaster,
                private ChangeListener, private ValueTree::Listener {
public:
    Project(const ValueTree &v, UndoManager &undoManager, ProcessorManager& processorManager, AudioDeviceManager& deviceManager)
            : state(v), undoManager(undoManager), processorManager(processorManager), deviceManager(deviceManager) {
        if (!state.isValid()) {
            state = createDefaultProject();
        } else {
            input = state.getChildWithName(IDs::INPUT);
            output = state.getChildWithName(IDs::OUTPUT);
            tracks = state.getChildWithName(IDs::TRACKS);
            masterTrack = tracks.getChildWithName(IDs::MASTER_TRACK);
            connections = state.getChildWithName(IDs::CONNECTIONS);
        }
        deviceManager.addChangeListener(this);
        jassert(state.hasType(IDs::PROJECT));
        state.addListener(this);
    }

    ~Project() override {
        state.removeListener(this);
        deviceManager.removeChangeListener(this);
    }

    ValueTree& getState() {
        return state;
    }

    UndoManager& getUndoManager() {
        return undoManager;
    }

    ValueTree& getConnections() {
        return connections;
    }

    ValueTree& getInput() {
        return input;
    }

    ValueTree& getOutput() {
        return output;
    }

    Array<ValueTree> getConnectionsForNode(AudioProcessorGraph::NodeID nodeId) {
        Array<ValueTree> nodeConnections;
        for (const auto& connection : connections) {
            if (connection.getChildWithProperty(IDs::nodeId, int(nodeId)).isValid()) {
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
        processor.setProperty(IDs::selected, true, nullptr);
    }

    bool hasConnections() {
        return connections.isValid() && connections.getNumChildren() > 0;
    }

    const ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) {
        for (auto connectionState : connections) {
            auto sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto destState = connectionState.getChildWithName(IDs::DESTINATION);
            if (AudioProcessorGraph::NodeID(int(sourceState[IDs::nodeId])) == connection.source.nodeID &&
                AudioProcessorGraph::NodeID(int(destState[IDs::nodeId])) == connection.destination.nodeID &&
                int(sourceState[IDs::channel]) == connection.source.channelIndex &&
                int(destState[IDs::channel]) == connection.destination.channelIndex) {
                return connectionState;
            }
        }
        return {};

    }

    // checks for duplicate add should be done before! (not done here to avoid redundant checks)
    void addConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool isDefault=true) {
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

    bool removeConnection(const ValueTree& connection, UndoManager* undoManager, bool defaults, bool custom) {
        if (connection.isValid() &&
            ((custom && connection.hasProperty(IDs::isCustomConnection)) ||
             (defaults && !connection.hasProperty(IDs::isCustomConnection)))) {
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
                    child.setProperty(IDs::processorSlot, slotForNodeIdSnapshot.at(int(child[IDs::nodeId])), nullptr);
                }
            }
        }
    }

    void moveSelectionUp() {}

    void moveSelectionDown() {}

    void moveSelectionLeft() {}

    void moveSelectionRight() {}

    void deleteSelectedItems() {
        deleteAllSelectedItems(state);
    }

    void deleteItem(const ValueTree &v, bool undoable=true) {
        if (!isItemDeletable(v))
            return;
        if (v.getParent().isValid()) {
            if (v.hasType(IDs::PROCESSOR)) {
                sendProcessorWillBeDestroyedMessage(v);
            }
            v.getParent().removeChild(v, undoable ? &undoManager : nullptr);
            if (v.hasType(IDs::PROCESSOR)) {
                sendProcessorHasBeenDestroyedMessage(v);
            }
        }
    }

    ValueTree createDefaultProject() {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        Helpers::createUuidProperty(state);

        input = ValueTree(IDs::INPUT);
        {
            PluginDescription &audioInputDescription = processorManager.getAudioInputDescription();
            ValueTree inputProcessor(IDs::PROCESSOR);
            Helpers::createUuidProperty(inputProcessor);
            inputProcessor.setProperty(IDs::id, audioInputDescription.createIdentifierString(), nullptr);
            inputProcessor.setProperty(IDs::name, audioInputDescription.name, nullptr);
            input.addChild(inputProcessor, -1, nullptr);
        }
        state.addChild(input, -1, nullptr);

        output = ValueTree(IDs::OUTPUT);
        {
            PluginDescription &audioOutputDescription = processorManager.getAudioOutputDescription();
            ValueTree outputProcessor(IDs::PROCESSOR);
            Helpers::createUuidProperty(outputProcessor);
            outputProcessor.setProperty(IDs::id, audioOutputDescription.createIdentifierString(), nullptr);
            outputProcessor.setProperty(IDs::name, audioOutputDescription.name, nullptr);
            output.addChild(outputProcessor, -1, nullptr);
        }
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
        if (selectedTrackHasMixerChannel() && description.name == MixerChannelProcessor::name())
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
        if (description.name == MixerChannelProcessor::name()) {
            insertIndex = -1;
            slot = NUM_AVAILABLE_PROCESSOR_SLOTS - 1;
            processor.setProperty(IDs::processorSlot, slot, nullptr);
        } else if (slot == -1) {
            // Insert new processors _right before_ the first MixerChannel processor, or at the end if there isn't one.
            insertIndex = getMaxProcessorInsertIndex(track);
            slot = 0;
            if (track.getNumChildren() > 1) {
                slot = int(track.getChild(track.getNumChildren() - 2)[IDs::processorSlot]) + 1;
            }
            processor.setProperty(IDs::processorSlot, slot, nullptr);
        } else {
            processor.setProperty(IDs::processorSlot, slot, nullptr);
            insertIndex = getParentIndexForProcessor(track, processor, nullptr);
        }

        track.addChild(processor, insertIndex, undoable ? &undoManager : nullptr);
        sendProcessorCreatedMessage(processor);

        return processor;
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

    const bool selectedTrackHasMixerChannel() const {
        return getMixerChannelProcessorForSelectedTrack().isValid();
    }

    int getMaxProcessorInsertIndex(const ValueTree& track) {
        const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
        return mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren() - 1;
    }

    void makeSlotsValid(const ValueTree& parent, UndoManager *undoManager) {
        std::__1::vector<int> slots;
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
                child.setProperty(IDs::processorSlot, *(iterator++), undoManager);
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
                            otherProcessorState.setProperty(IDs::processorSlot, otherSlot - 1, undoManager);
                            return currentIndex + 2;
                        } else {
                            otherProcessorState.setProperty(IDs::processorSlot, otherSlot + 1, undoManager);
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
        ProjectChangeBroadcaster::sendItemRemovedMessage(item);
        if (item == selectedTrack) {
            selectedTrack = ValueTree();
        }
        if (item == selectedProcessor) {
            selectedProcessor = ValueTree();
        }
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

    const static int NUM_VISIBLE_TRACKS = 8;
    const static int NUM_VISIBLE_PROCESSOR_SLOTS = 10;
    // first row is reserved for audio input, last row for audio output. second-to-last is horizontal master track.
    const static int NUM_AVAILABLE_PROCESSOR_SLOTS = NUM_VISIBLE_PROCESSOR_SLOTS - 3;
private:
    ValueTree state;
    UndoManager &undoManager;
    ValueTree input, output, tracks, masterTrack, selectedTrack, selectedProcessor, connections;

    Array<ValueTree> connectionsSnapshot;
    std::unordered_map<int, int> slotForNodeIdSnapshot;

    ProcessorManager &processorManager;
    AudioDeviceManager& deviceManager;
    
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
            syncInputDevicesWithDeviceManager();
            syncOutputDevicesWithDeviceManager();
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
            deleteItem(inputChild, false);
        }
        for (const auto& deviceName : MidiInput::getDevices()) {
            if (deviceManager.isMidiInputEnabled(deviceName) &&
                !input.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiInputProcessor(IDs::PROCESSOR);
                Helpers::createUuidProperty(midiInputProcessor);
                midiInputProcessor.setProperty(IDs::id, MidiInputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiInputProcessor.setProperty(IDs::name, MidiInputProcessor::name(), nullptr);
                midiInputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                input.addChild(midiInputProcessor, -1, nullptr);
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
            deleteItem(outputChild, false);
        }
        for (const auto& deviceName : MidiOutput::getDevices()) {
            if (deviceManager.isMidiOutputEnabled(deviceName) &&
                !output.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiOutputProcessor(IDs::PROCESSOR);
                Helpers::createUuidProperty(midiOutputProcessor);
                midiOutputProcessor.setProperty(IDs::id, MidiOutputProcessor::getPluginDescription().createIdentifierString(), nullptr);
                midiOutputProcessor.setProperty(IDs::name, MidiOutputProcessor::name(), nullptr);
                midiOutputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                output.addChild(midiOutputProcessor, -1, nullptr);
            }
        }
    }

    void deleteAllSelectedItems(const ValueTree &parent) {
        Array<ValueTree> allSelectedItems = findAllItemsWithPropertyRecursive(parent, IDs::selected, true);
        for (const auto &selectedItem : allSelectedItems) {
            deleteItem(selectedItem, true);
        }
    }

    static inline bool isItemDeletable(const ValueTree& item) {
        return !item.hasType(IDs::MASTER_TRACK);
    }

    void valueTreeChildAdded(ValueTree &, ValueTree &tree) override {
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int) override {
        sendItemRemovedMessage(tree);
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected && tree[IDs::selected]) {
            deselectAllItemsExcept(state, tree);
            sendItemSelectedMessage(tree);
        }
    }

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}

    static void deselectAllItemsExcept(const ValueTree& parent, const ValueTree& except) {
        for (auto child : parent) {
            if (child[IDs::selected] && child != except) {
                child.setProperty(IDs::selected, false, nullptr);
            } else {
                deselectAllItemsExcept(child, except);
            }
        }
    }
    
    static ValueTree findFirstItemWithPropertyRecursive(const ValueTree &parent, const Identifier &i, const var &value) {
        for (const auto& child : parent) {
            if (child[i] == value)
                return child;
        }
        for (const auto& child : parent) {
            const ValueTree &match = findFirstItemWithPropertyRecursive(child, i, value);
            if (match.isValid()) {
                return match;
            }
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
