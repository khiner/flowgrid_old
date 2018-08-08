#pragma once

#include <unordered_map>
#include <push2/Push2MidiCommunicator.h>
#include "ValueTreeItems.h"
#include "processors/ProcessorManager.h"
#include "Utilities.h"

class Project : public FileBasedDocument, public ProjectChangeBroadcaster,
                private ChangeListener, private ValueTree::Listener {
public:
    Project(UndoManager &undoManager, ProcessorManager& processorManager, AudioDeviceManager& deviceManager)
            : FileBasedDocument(getFilenameSuffix(), getFilenameWildcard(), "Load a project", "Save a project"),
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

        undoManager.addChangeListener(this);
        RecentlyOpenedFilesList recentFiles;
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

    UndoManager& getUndoManager() { return undoManager; }

    ValueTree& getState() { return state; }

    ValueTree& getConnections() { return connections; }

    ValueTree& getInput() { return input; }

    ValueTree& getOutput() { return output; }

    ValueTree& getTracks() { return tracks; }

    int getNumTracks() { return tracks.getNumChildren(); }

    ValueTree getTrack(int trackIndex) { return tracks.getChild(trackIndex); }

    ValueTree getMasterTrack() { return tracks.getChildWithName(IDs::MASTER_TRACK); }

    const ValueTree& getSelectedTrack() const { return selectedTrack; }

    ValueTree& getSelectedProcessor() { return selectedProcessor; }

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

    Array<ValueTree> getConnectionsForNode(AudioProcessorGraph::NodeID nodeId, bool audio=true, bool midi=true, bool incoming=true, bool outgoing=true) {
        Array<ValueTree> nodeConnections;
        for (const auto& connection : connections) {
            const auto &connectionType = connection.getChildWithProperty(IDs::nodeId, int(nodeId));

            if (!connectionType.isValid())
                continue;
            bool directionIsAcceptable = (incoming && connectionType.hasType(IDs::DESTINATION)) || (outgoing && connectionType.hasType(IDs::SOURCE));
            bool typeIsAcceptable = (audio && int(connectionType[IDs::channel]) != AudioProcessorGraph::midiChannelIndex) ||
                                    (midi && int(connectionType[IDs::channel]) == AudioProcessorGraph::midiChannelIndex);
            if (directionIsAcceptable && typeIsAcceptable) {
                nodeConnections.add(connection);
            }
        }

        return nodeConnections;
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
        if (!connection.isValid())
            return false;

        bool customIsAcceptable = (custom && connection.hasProperty(IDs::isCustomConnection)) ||
                                  (defaults && !connection.hasProperty(IDs::isCustomConnection));
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
            if (v.hasType(IDs::TRACK) || v.hasType(IDs::MASTER_TRACK)) {
                while (v.getNumChildren() > 0)
                    deleteItem(v.getChild(v.getNumChildren() - 1), undoable);
            }
            if (v.hasType(IDs::PROCESSOR)) {
                sendProcessorWillBeDestroyedMessage(v);
            }
            v.getParent().removeChild(v, undoable ? &undoManager : nullptr);
            if (v.hasType(IDs::PROCESSOR)) {
                sendProcessorHasBeenDestroyedMessage(v);
            }
        }
    }

    void createDefaultProject() {
        createAudioIoProcessors();

        ValueTree masterTrack(IDs::MASTER_TRACK);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        tracks.addChild(masterTrack, -1, nullptr);

        ValueTree masterMixer = createAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack, -1, false);
        masterMixer.setProperty(IDs::selected, true, nullptr);

        ValueTree track = createAndAddTrack(false);
        createAndAddProcessor(SineBank::getPluginDescription(), track, -1, false);
    }

    ValueTree createAndAddTrack(bool undoable=true, bool addMixer=true, ValueTree nextToTrack={}) {
        int numTracks = getNumTracks() - 1; // minus 1 because of master track

        if (!nextToTrack.isValid()) {
            if (selectedTrack.isValid()) {
                // If a track is selected, insert the new track to the left of it if there's no mixer,
                // or to the right of the first track with a mixer if the new track has a mixer.
                nextToTrack = selectedTrack;

                if (addMixer) {
                    while (nextToTrack.isValid() && !getMixerChannelProcessorForTrack(nextToTrack).isValid())
                        nextToTrack = nextToTrack.getSibling(1);
                }
            } else if (getMasterTrack().isValid()) {
                nextToTrack = getMasterTrack();
            }
        }
        ValueTree track(IDs::TRACK);
        track.setProperty(IDs::name, (addMixer || numTracks == 0) ? ("Track " + String(numTracks + 1)) : makeTrackNameUnique(nextToTrack[IDs::name]), nullptr);
        track.setProperty(IDs::colour, (addMixer || numTracks == 0) ? Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString() : nextToTrack[IDs::colour].toString(), nullptr);

        tracks.addChild(track,
                nextToTrack.isValid() ? nextToTrack.getParent().indexOf(nextToTrack) + (addMixer ? 1 : 0): -1,
                undoable ? &undoManager : nullptr);

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
        if (description.name == MixerChannelProcessor::name() && getMixerChannelProcessorForTrack(track).isValid())
            return ValueTree(); // only one mixer channel per track

        if (processorManager.isGeneratorOrInstrument(&description) &&
            processorManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            return createAndAddProcessor(description, createAndAddTrack(undoable, false, track), slot, undoable);
        }

        ValueTree processor(IDs::PROCESSOR);
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
        if (item.hasType(IDs::TRACK))
            selectedTrack = item;
        else if (item.getParent().hasType(IDs::TRACK))
            selectedTrack = item.getParent();
        else
            selectedTrack = ValueTree();

        if (item.hasType(IDs::PROCESSOR))
            selectedProcessor = item;
        else
            selectedProcessor = ValueTree();

        ProjectChangeBroadcaster::sendItemSelectedMessage(item);
    }

    void sendItemRemovedMessage(ValueTree item) override {
        ProjectChangeBroadcaster::sendItemRemovedMessage(item);
        if (item == selectedTrack)
            selectedTrack = ValueTree();
        if (item == selectedProcessor)
            selectedProcessor = ValueTree();
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

    String getAudioChannelName(int channelIndex, bool input) {
        if (auto *audioDevice = deviceManager.getCurrentAudioDevice()) {
            auto channelNames = input ? audioDevice->getInputChannelNames() : audioDevice->getOutputChannelNames();
            auto activeChannels = input ? audioDevice->getActiveInputChannels()
                                        : audioDevice->getActiveOutputChannels();
            int activeIndex = 0;
            for (int i = 0; i < channelNames.size(); i++) {
                if (activeChannels[i] && activeIndex++ == channelIndex) {
                    return channelNames[i];
                }
            }
        }
        return "";
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
        const String& inputDeviceName = newState.getChildWithName(IDs::INPUT)[IDs::deviceName];
        const String& outputDeviceName = newState.getChildWithName(IDs::OUTPUT)[IDs::deviceName];

        static const String& failureMessage = TRANS("Could not open an Audio IO device used by this project."
                                                    "All connections with the missing device will be gone.  "
                                                    "If you want this project to look like it did when you saved it, "
                                                    "the best thing to do is to reconnect the missing device and "
                                                    "reload this project (without saving first!).");
        if (isDeviceWithNamePresent(inputDeviceName))
            input.setProperty(IDs::deviceName, inputDeviceName, nullptr);
        else
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, TRANS("Failed to open input device \"") + inputDeviceName + "\"", failureMessage);

        if (isDeviceWithNamePresent(outputDeviceName))
            output.setProperty(IDs::deviceName, outputDeviceName, nullptr);
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
                if (processorState.hasType(IDs::PROCESSOR)) {
                    if (auto* node = graph->getNodeForId(AudioProcessorGraph::NodeID(int(processorState[IDs::nodeId])))) {
                        MemoryBlock memoryBlock;
                        if (auto* processor = node->getProcessor()) {
                            processor->getStateInformation(memoryBlock);
                            processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
                        }
                    }
                }
            }
        }
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

    static const char* getFilenameSuffix() { return ".smp"; }
    static const char* getFilenameWildcard() { return "*.smp"; }
    
    const static int NUM_VISIBLE_TRACKS = 8;
    const static int NUM_VISIBLE_PROCESSOR_SLOTS = 10;
    // first row is reserved for audio input, last row for audio output. second-to-last is horizontal master track.
    const static int NUM_AVAILABLE_PROCESSOR_SLOTS = NUM_VISIBLE_PROCESSOR_SLOTS - 3;
private:
    ValueTree state;
    UndoManager &undoManager;
    AudioProcessorGraph* graph;
    ValueTree input, output, tracks, selectedTrack, selectedProcessor, connections;

    Array<ValueTree> connectionsSnapshot;
    std::unordered_map<int, int> slotForNodeIdSnapshot;

    ProcessorManager &processorManager;
    AudioDeviceManager& deviceManager;
    
    void clear() {
        sendItemSelectedMessage({});
        input.removeAllChildren(nullptr);
        output.removeAllChildren(nullptr);
        while (tracks.getNumChildren() > 0)
            deleteItem(tracks.getChild(tracks.getNumChildren() - 1), false);
        connections.removeAllChildren(nullptr);
        undoManager.clearUndoHistory();
    }

    void createAudioIoProcessors() {
        {
            PluginDescription &audioInputDescription = processorManager.getAudioInputDescription();
            ValueTree inputProcessor(IDs::PROCESSOR);
            inputProcessor.setProperty(IDs::id, audioInputDescription.createIdentifierString(), nullptr);
            inputProcessor.setProperty(IDs::name, audioInputDescription.name, nullptr);
            input.addChild(inputProcessor, -1, nullptr);
        }
        {
            PluginDescription &audioOutputDescription = processorManager.getAudioOutputDescription();
            ValueTree outputProcessor(IDs::PROCESSOR);
            outputProcessor.setProperty(IDs::id, audioOutputDescription.createIdentifierString(), nullptr);
            outputProcessor.setProperty(IDs::name, audioOutputDescription.name, nullptr);
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
            deleteItem(inputChild, false);
        }
        for (const auto& deviceName : MidiInput::getDevices()) {
            if (deviceManager.isMidiInputEnabled(deviceName) &&
                !input.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                ValueTree midiInputProcessor(IDs::PROCESSOR);
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
//        return !item.hasType(IDs::MASTER_TRACK);
        return true;
    }

    void valueTreeChildAdded(ValueTree &, ValueTree &tree) override {}

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int) override {
        sendItemRemovedMessage(tree);
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected && tree[IDs::selected]) {
            sendItemSelectedMessage(tree);
        } else if (i == IDs::deviceName && (tree == input || tree == output)) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            const String &deviceName = tree[IDs::deviceName];

            if (tree == input) {
                config.inputDeviceName = deviceName;
            } else if (tree == output) {
                config.outputDeviceName = deviceName;
            }
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}
    
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
