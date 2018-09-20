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

class Project : public FileBasedDocument, public ProjectChangeBroadcaster, public StatefulAudioProcessorContainer,
                private ChangeListener, private ValueTree::Listener {
public:
    Project(UndoManager &undoManager, ProcessorManager& processorManager, AudioDeviceManager& deviceManager)
            : FileBasedDocument(getFilenameSuffix(), "*" + getFilenameSuffix(), "Load a project", "Save project"),
              processorManager(processorManager),
              tracksManager(viewManager, *this, *this, processorManager, undoManager), connectionsManager(*this),
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

    TracksStateManager& getTracksManager() { return tracksManager; }

    ConnectionsStateManager& getConnectionHelper() { return connectionsManager; }

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
            tracksManager.startRectangleSelection();
        this->shiftHeld = shiftHeld;
        if (!isShiftHeld())
            tracksManager.endRectangleSelection();
    }

    void setPush2ShiftHeld(bool shiftHeld) {
        if (!isShiftHeld() && shiftHeld)
            tracksManager.startRectangleSelection();
        this->push2ShiftHeld = shiftHeld;
        if (!isShiftHeld())
            tracksManager.endRectangleSelection();
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

    void navigateUp() { tracksManager.findItemToSelectWithUpDownDelta(-1).select(tracksManager); }

    void navigateDown() { tracksManager.findItemToSelectWithUpDownDelta(1).select(tracksManager); }

    void navigateLeft() { tracksManager.findItemToSelectWithLeftRightDelta(-1).select(tracksManager); }

    void navigateRight() { tracksManager.findItemToSelectWithLeftRightDelta(1).select(tracksManager); }

    bool canNavigateUp() const { return tracksManager.findItemToSelectWithUpDownDelta(-1).isValid(); }

    bool canNavigateDown() const { return tracksManager.findItemToSelectWithUpDownDelta(1).isValid(); }

    bool canNavigateLeft() const { return tracksManager.findItemToSelectWithLeftRightDelta(-1).isValid(); }

    bool canNavigateRight() const { return tracksManager.findItemToSelectWithLeftRightDelta(1).isValid(); }

    void createDefaultProject() {
        viewManager.initializeDefault();
        createAudioIoProcessors();
        ValueTree track = tracksManager.createAndAddTrack(nullptr);
        tracksManager.createAndAddProcessor(SineBank::getPluginDescription(), track, nullptr, -1);
        tracksManager.createAndAddMasterTrack(nullptr, true);
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

        viewManager.getState().copyPropertiesFrom(newState.getChildWithName(IDs::VIEW_STATE), nullptr);

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
        Utilities::moveAllChildren(newState.getChildWithName(IDs::TRACKS), tracksManager.getState(), nullptr);
        Utilities::moveAllChildren(newState.getChildWithName(IDs::CONNECTIONS), connectionsManager.getState(), nullptr);

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
    ConnectionsStateManager connectionsManager;

    UndoManager &undoManager;
    ValueTree input, output;

    AudioDeviceManager& deviceManager;

    AudioProcessorGraph* graph {};
    StatefulAudioProcessorContainer* statefulAudioProcessorContainer {};

    bool shiftHeld { false }, push2ShiftHeld { false };

    void clear() {
        input.removeAllChildren(nullptr);
        output.removeAllChildren(nullptr);
        tracksManager.clear();
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
        for (auto inputChild : input) {
            if (inputChild.hasProperty(IDs::deviceName)) {
                const String &deviceName = inputChild[IDs::deviceName];
                if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                    inputChildrenToDelete.add(inputChild);
                }
            }
        }
        for (const auto& inputChild : inputChildrenToDelete) {
            tracksManager.deleteTrackOrProcessor(inputChild, &undoManager);
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
            tracksManager.deleteTrackOrProcessor(outputChild, &undoManager);
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
