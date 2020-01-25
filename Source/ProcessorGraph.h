#pragma once

#include <Identifiers.h>
#include <view/PluginWindow.h>
#include <processors/StatefulAudioProcessorWrapper.h>

#include "JuceHeader.h"
#include "processors/ProcessorManager.h"
#include "Project.h"
#include "state_managers/ConnectionsStateManager.h"
#include "StatefulAudioProcessorContainer.h"
#include "push2/Push2MidiCommunicator.h"

class ProcessorGraph : public AudioProcessorGraph, public StatefulAudioProcessorContainer, private ValueTree::Listener,
                       private ProcessorLifecycleListener, private Timer {
public:
    explicit ProcessorGraph(Project &project, ConnectionsStateManager& connectionsManager,
                            UndoManager &undoManager, AudioDeviceManager& deviceManager,
                            Push2MidiCommunicator& push2MidiCommunicator)
            : undoManager(undoManager), project(project), viewManager(project.getViewStateManager()),
            tracksManager(project.getTracksManager()), connectionsManager(connectionsManager),
            deviceManager(deviceManager), push2MidiCommunicator(push2MidiCommunicator) {
        enableAllBuses();

        project.getState().addListener(this);
        viewManager.getState().addListener(this);
        project.setStatefulAudioProcessorContainer(this);
        tracksManager.addProcessorLifecycleListener(this);
    }

    ~ProcessorGraph() override {
        project.setStatefulAudioProcessorContainer(nullptr);
        tracksManager.removeProcessorLifecycleListener(this);
        project.getState().removeListener(this);
    }

    StatefulAudioProcessorWrapper* getProcessorWrapperForNodeId(NodeID nodeId) const override {
        if (!nodeId.isValid())
            return nullptr;

        auto nodeIdAndProcessorWrapper = processorWrapperForNodeId.find(nodeId);
        if (nodeIdAndProcessorWrapper == processorWrapperForNodeId.end())
            return nullptr;

        return nodeIdAndProcessorWrapper->second.get();
    }

    Node* getNodeForState(const ValueTree &processorState) const {
        return getNodeForId(getNodeIdForState(processorState));
    }

    StatefulAudioProcessorWrapper *getMasterGainProcessor() {
        const ValueTree &gain = tracksManager.getMixerChannelProcessorForTrack(tracksManager.getMasterTrack());
        return getProcessorWrapperForState(gain);
    }

    ResizableWindow* getOrCreateWindowFor(Node* node, PluginWindow::Type type) {
        jassert(node != nullptr);

        for (auto* w : activePluginWindows)
            if (w->node->nodeID == node->nodeID && w->type == type)
                return w;

        if (auto* processor = node->getProcessor())
            return activePluginWindows.add(new PluginWindow(node, type, activePluginWindows));

        return nullptr;
    }

    bool closeAnyOpenPluginWindows() {
        bool wasEmpty = activePluginWindows.isEmpty();
        activePluginWindows.clear();
        return !wasEmpty;
    }

    void beginDraggingProcessor(const ValueTree& processor) {
        if (processor[IDs::name] == MixerChannelProcessor::name())
            // mixer channel processors are special processors.
            // they could be dragged and reconnected like any old processor, but please don't :)
            return;
        tracksManager.setCurrentlyDraggingProcessor(processor);
        project.makeConnectionsSnapshot();
    }

    void dragProcessorToPosition(const Point<int> &trackAndSlot) {
        if (tracksManager.getCurrentlyDraggingTrackAndSlot() != trackAndSlot &&
            trackAndSlot.y < tracksManager.getMixerChannelSlotForTrack(tracksManager.getTrack(trackAndSlot.x))) {
            tracksManager.setCurrentlyDraggingTrackAndSlot(trackAndSlot);

            if (tracksManager.moveProcessor(trackAndSlot, getDragDependentUndoManager())) {
                if (tracksManager.getCurrentlyDraggingTrackAndSlot() == tracksManager.getInitialDraggingTrackAndSlot())
                    project.restoreConnectionsSnapshot();
                else
                    updateAllDefaultConnections();
            }
        }
    }

    void endDraggingProcessor() {
        if (tracksManager.isCurrentlyDraggingProcessor() && tracksManager.getCurrentlyDraggingTrackAndSlot() != tracksManager.getInitialDraggingTrackAndSlot()) {
            // update the audio graph to match the current preview UI graph.
            tracksManager.moveProcessor(tracksManager.getInitialDraggingTrackAndSlot(), nullptr);
            project.restoreConnectionsSnapshot();
            tracksManager.moveProcessor(tracksManager.getCurrentlyDraggingTrackAndSlot(), &undoManager);
            tracksManager.setCurrentlyDraggingProcessor({});
            if (project.isShiftHeld()) {
                project.setShiftHeld(false);
                updateAllDefaultConnections(true);
                project.setShiftHeld(true);
            } else {
                updateAllDefaultConnections();
            }
        }
    }

    bool canConnectUi(const Connection& c) const {
        if (auto* source = getNodeForId(c.source.nodeID))
            if (auto* dest = getNodeForId(c.destination.nodeID))
                return canConnectUi(source, c.source.channelIndex, dest, c.destination.channelIndex);

        return false;
    }

    bool canConnectUi(Node* source, int sourceChannel, Node* dest, int destChannel) const noexcept {
        bool sourceIsMIDI = sourceChannel == midiChannelIndex;
        bool destIsMIDI   = destChannel == midiChannelIndex;

        if (sourceChannel < 0
            || destChannel < 0
            || source == dest
            || sourceIsMIDI != destIsMIDI)
            return false;

        if (source == nullptr
            || (!sourceIsMIDI && sourceChannel >= source->getProcessor()->getTotalNumOutputChannels())
            || (sourceIsMIDI && ! source->getProcessor()->producesMidi()))
            return false;

        if (dest == nullptr
            || (!destIsMIDI && destChannel >= dest->getProcessor()->getTotalNumInputChannels())
            || (destIsMIDI && ! dest->getProcessor()->acceptsMidi()))
            return false;

        return !connectionsManager.getConnectionMatching({{source->nodeID, sourceChannel}, {dest->nodeID, destChannel}}).isValid();
    }

    bool removeConnection(const Connection& c) override {
        const ValueTree &connectionState = connectionsManager.getConnectionMatching(c);
        bool removed = checkedRemoveConnection(c, getDragDependentUndoManager(), true, true);
        if (removed && connectionState.hasProperty(IDs::isCustomConnection)) {
            const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto nodeId = getNodeIdForState(sourceState);
            const auto& processor = getProcessorStateForNodeId(nodeId);
            jassert(processor.isValid());
            updateDefaultConnectionsForProcessor(processor, true);
        }
        return removed;
    }

    bool addConnection(const Connection& c) override {
        return checkedAddConnection(c, false, getDragDependentUndoManager());
    }

    bool addDefaultConnection(const Connection& c, UndoManager* undoManager) {
        return checkedAddConnection(c, true, undoManager);
    }

    bool addCustomConnection(const Connection& c) {
        return checkedAddConnection(c, false, &undoManager);
    }

    bool removeDefaultConnection(const Connection& c) {
        return checkedRemoveConnection(c, getDragDependentUndoManager(), true, false);
    }

    void setDefaultConnectionsAllowed(NodeID nodeId, bool defaultConnectionsAllowed) {
        auto processor = getProcessorStateForNodeId(nodeId);
        jassert(processor.isValid());
        processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, &undoManager);
    }

    bool disconnectNode(NodeID nodeId) override {
        return doDisconnectNode(nodeId, all, getDragDependentUndoManager(), true, true, true, true);
    }

    bool disconnectDefaults(NodeID nodeId) {
        return doDisconnectNode(nodeId, all, getDragDependentUndoManager(), true, false, true, true);
    }

    bool disconnectDefaultOutgoing(NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager) {
        return doDisconnectNode(nodeId, connectionType, undoManager, true, false, false, true);
    }

    bool disconnectCustom(NodeID nodeId) {
        return doDisconnectNode(nodeId, all, getDragDependentUndoManager(), false, true, true, true);
    }

    UndoManager &undoManager;
private:
    std::map<NodeID, std::unique_ptr<StatefulAudioProcessorWrapper> > processorWrapperForNodeId;

    Project &project;
    TracksStateManager &tracksManager;
    ViewStateManager &viewManager;

    ConnectionsStateManager &connectionsManager;
    AudioDeviceManager &deviceManager;
    Push2MidiCommunicator &push2MidiCommunicator;
    
    OwnedArray<PluginWindow> activePluginWindows;

    Array<int> defaultAudioConnectionChannels {0, 1};
    Array<int> defaultMidiConnectionChannels {midiChannelIndex};

    bool isMoving { false };
    bool isDeleting { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    UndoManager* getDragDependentUndoManager() {
        return tracksManager.getDragDependentUndoManager();
    }

    bool checkedAddConnection(const Connection &c, bool isDefault, UndoManager* undoManager) {
        if (isDefault && project.isShiftHeld())
            return false; // no default connection stuff while shift is held
        if (canConnectUi(c)) {
            if (!isDefault)
                disconnectDefaultOutgoing(c.source.nodeID, c.source.isMIDI() ? midi : audio, getDragDependentUndoManager());
            connectionsManager.addConnection(c, undoManager, isDefault);
            return true;
        }
        return false;
    }

    bool checkedRemoveConnection(const ValueTree& connection, UndoManager* undoManager, bool allowDefaults, bool allowCustom) {
        if (!connection.isValid() || (!connection[IDs::isCustomConnection] && project.isShiftHeld()))
            return false; // no default connection stuff while shift is held
        return connectionsManager.removeConnection(connection, undoManager, allowDefaults, allowCustom);
    }

    bool checkedRemoveConnection(const Connection &connection, UndoManager* undoManager, bool defaults, bool custom) {
        const ValueTree &connectionState = connectionsManager.getConnectionMatching(connection);
        return checkedRemoveConnection(connectionState, undoManager, defaults, custom);
    }

    bool doDisconnectNode(NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager,
                          bool defaults, bool custom, bool incoming, bool outgoing) {
        const auto connections = connectionsManager.getConnectionsForNode(nodeId, connectionType, incoming, outgoing);
        bool anyRemoved = false;
        for (const auto &connection : connections) {
            if (checkedRemoveConnection(connection, undoManager, defaults, custom))
                anyRemoved = true;
        }

        return anyRemoved;
    }

    void addProcessor(const ValueTree &processorState) {
        static String errorMessage = "Could not create processor";
        auto description = project.getTypeForIdentifier(processorState[IDs::id]);
        auto processor = project.getFormatManager().createPluginInstance(*description, getSampleRate(), getBlockSize(), errorMessage);
        if (processorState.hasProperty(IDs::state)) {
            MemoryBlock memoryBlock;
            memoryBlock.fromBase64Encoding(processorState[IDs::state].toString());
            processor->setStateInformation(memoryBlock.getData(), (int) memoryBlock.getSize());
        }

        const Node::Ptr &newNode = processorState.hasProperty(IDs::nodeId) ?
                                   addNode(std::move(processor), getNodeIdForState(processorState)) :
                                   addNode(std::move(processor));
        processorWrapperForNodeId[newNode->nodeID] = std::make_unique<StatefulAudioProcessorWrapper>
                (dynamic_cast<AudioPluginInstance *>(newNode->getProcessor()), newNode->nodeID, processorState, undoManager, project.getDeviceManager());
        if (processorWrapperForNodeId.size() == 1)
            // Added the first processor. Start the timer that flushes new processor state to their value trees.
            startTimerHz(10);

        if (auto midiInputProcessor = dynamic_cast<MidiInputProcessor *>(newNode->getProcessor())) {
            const String &deviceName = processorState.getProperty(IDs::deviceName);
            midiInputProcessor->setDeviceName(deviceName);
            if (deviceName.containsIgnoreCase(push2MidiDeviceName)) {
                push2MidiCommunicator.addMidiInputCallback(&midiInputProcessor->getMidiMessageCollector());
            } else {
                deviceManager.addMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
            }
        } else if (auto *midiOutputProcessor = dynamic_cast<MidiOutputProcessor *>(newNode->getProcessor())) {
            const String &deviceName = processorState.getProperty(IDs::deviceName);
            if (auto* enabledMidiOutput = deviceManager.getEnabledMidiOutput(deviceName))
                midiOutputProcessor->setMidiOutput(enabledMidiOutput);
        }
        ValueTree mutableProcessor = processorState;
        if (mutableProcessor.hasProperty(IDs::processorInitialized))
            mutableProcessor.sendPropertyChangeMessage(IDs::processorInitialized);
        else
            mutableProcessor.setProperty(IDs::processorInitialized, true, nullptr);

    }

    void removeProcessor(const ValueTree &processor) {
        auto* processorWrapper = getProcessorWrapperForState(processor);
        jassert(processorWrapper);
        const NodeID nodeId = getNodeIdForState(processor);

        // disconnect should have already been called before delete! (to avoid nested undo actions)
        if (processor[IDs::name] == MidiInputProcessor::name()) {
            if (auto *midiInputProcessor = dynamic_cast<MidiInputProcessor *>(processorWrapper->processor)) {
                const String &deviceName = processor.getProperty(IDs::deviceName);
                if (deviceName.containsIgnoreCase(push2MidiDeviceName)) {
                    push2MidiCommunicator.removeMidiInputCallback(&midiInputProcessor->getMidiMessageCollector());
                } else {
                    deviceManager.removeMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
                }
            }
        }
        processorWrapperForNodeId.erase(nodeId);
        nodes.removeObject(getNodeForId(nodeId));
        topologyChanged();
    }

    // Disconnect external audio/midi inputs
    // If `addDefaultConnections` is true, then for both audio and midi connection types:
    //   * Find the topmost effect processor (receiving audio/midi) in the focused track
    //   * Connect external device inputs to its most-upstream connected processor (including itself)
    // (Note that it is possible for the same focused track to have a default audio-input processor different
    // from its default midi-input processor.)
    void resetDefaultExternalInputs(UndoManager* undoManager, bool addDefaultConnections=true) {
        auto audioInputNodeId = getNodeIdForState(project.getAudioInputProcessorState());
        disconnectDefaultOutgoing(audioInputNodeId, audio, undoManager);

        for (const auto& midiInputProcessor : project.getInput()) {
            if (midiInputProcessor.getProperty(IDs::name) == MidiInputProcessor::name()) {
                const auto nodeId = getNodeIdForState(midiInputProcessor);
                disconnectDefaultOutgoing(nodeId, midi, undoManager);
            }
        }

        if (addDefaultConnections) {
            ValueTree processorToReceiveExternalInput = findEffectProcessorToReceiveDefaultExternalInput(audio);
            if (processorToReceiveExternalInput.isValid())
                defaultConnect(audioInputNodeId, getNodeIdForState(processorToReceiveExternalInput), audio, undoManager);

            for (const auto& midiInputProcessor : project.getInput()) {
                if (midiInputProcessor.getProperty(IDs::name) == MidiInputProcessor::name()) {
                    const auto midiInputNodeId = getNodeIdForState(midiInputProcessor);
                    processorToReceiveExternalInput = findEffectProcessorToReceiveDefaultExternalInput(midi);
                    if (processorToReceiveExternalInput.isValid())
                        defaultConnect(midiInputNodeId, getNodeIdForState(processorToReceiveExternalInput), midi, undoManager);
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
            if (isProcessorAnEffect(processor, connectionType)) {
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
        NodeID processorNodeId = getNodeIdForState(processor);
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

    // Connect the given processor to the appropriate default external device input.
    void defaultConnect(NodeID fromNodeId, NodeID toNodeId, ConnectionType connectionType, UndoManager *undoManager) {
        if (toNodeId.isValid()) {
            const auto &defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
            for (auto channel : defaultConnectionChannels) {
                addDefaultConnection({{fromNodeId, channel}, {toNodeId, channel}}, undoManager);
            }
        }
    }

    inline const Array<int>& getDefaultConnectionChannels(ConnectionType connectionType) const {
        return connectionType == audio ? defaultAudioConnectionChannels : defaultMidiConnectionChannels;
    }

    ValueTree findProcessorToFlowInto(const ValueTree &track, const ValueTree &processor, ConnectionType connectionType) {
        if (!processor.hasType(IDs::PROCESSOR) || !isProcessorAProducer(processor, connectionType))
            return {};

        int siblingDelta = 0;
        ValueTree otherTrack;
        while ((otherTrack = track.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : otherTrack) {
                if (otherProcessor == processor) continue;
                bool isOtherProcessorBelow = int(otherProcessor[IDs::processorSlot]) > int(processor[IDs::processorSlot]) ||
                                             (track != otherTrack && TracksStateManager::isMasterTrack(otherTrack));
                if (!isOtherProcessorBelow) continue;
                if (canProcessorDefaultConnectTo(processor, otherProcessor, connectionType) ||
                    // If a non-effect (another producer) is under this processor in the same track, and no effect processors
                    // to the right have a lower slot, block this producer's output by the other producer,
                    // effectively replacing it.
                    track == otherTrack) {
                    return otherProcessor;
                }
            }
        }

        return project.getAudioOutputProcessorState();
    }

    bool isProcessorAProducer(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumOutputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->producesMidi());
        }
        return false;
    }

    bool isProcessorAnEffect(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumInputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->acceptsMidi());
        }
        return false;
    }

    bool canProcessorDefaultConnectTo(const ValueTree &processor, const ValueTree &otherProcessor, ConnectionType connectionType) const {
        if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor)
            return false;

        return isProcessorAProducer(processor, connectionType) && isProcessorAnEffect(otherProcessor, connectionType);
    }

    void recursivelyInitializeState(const ValueTree &state) {
        if (state.hasType(IDs::PROCESSOR)) {
            addProcessor(state);
            return;
        }
        for (const ValueTree& child : state) {
            recursivelyInitializeState(child);
        }
    }
    
    void updateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom=false) {
        for (const auto& track : project.getTracks()) {
            for (const auto& processor : track) {
                updateDefaultConnectionsForProcessor(processor, false, makeInvalidDefaultsIntoCustom);
            }
        }
        resetDefaultExternalInputs(getDragDependentUndoManager());
    }
    
    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool updateExternalInputs, bool makeInvalidDefaultsIntoCustom=false) {
        for (auto connectionType : {audio, midi}) {
            NodeID nodeId = getNodeIdForState(processor);
            if (!processor[IDs::allowDefaultConnections]) {
                disconnectDefaults(nodeId);
                return;
            }
            auto outgoingCustomConnections = connectionsManager.getConnectionsForNode(nodeId, connectionType, false, true, true, false);
            if (!outgoingCustomConnections.isEmpty()) {
                disconnectDefaultOutgoing(nodeId, connectionType, getDragDependentUndoManager());
                return;
            }
            auto outgoingDefaultConnections = connectionsManager.getConnectionsForNode(nodeId, connectionType, false, true, false, true);
            auto processorToConnectTo = findProcessorToFlowInto(processor.getParent(), processor, connectionType);
            auto nodeIdToConnectTo = getNodeIdForState(processorToConnectTo);

            const auto &defaultChannels = getDefaultConnectionChannels(connectionType);
            bool anyCustomAdded = false;
            for (const auto &connection : outgoingDefaultConnections) {
                NodeID nodeIdCurrentlyConnectedTo = getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
                if (nodeIdCurrentlyConnectedTo != nodeIdToConnectTo) {
                    for (auto channel : defaultChannels) {
                        removeDefaultConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}});
                    }
                    if (makeInvalidDefaultsIntoCustom) {
                        for (auto channel : defaultChannels) {
                            if (addCustomConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}}))
                                anyCustomAdded = true;
                        }
                    }
                }
            }
            if (!anyCustomAdded) {
                if (processor[IDs::allowDefaultConnections] && processorToConnectTo[IDs::allowDefaultConnections]) {
                    for (auto channel : defaultChannels) {
                        addDefaultConnection({{nodeId, channel}, {nodeIdToConnectTo, channel}}, getDragDependentUndoManager());
                    }
                }
            }
        }
        if (updateExternalInputs) {
            resetDefaultExternalInputs(getDragDependentUndoManager());
        }
    }
    
    void valueTreePropertyChanged(ValueTree& tree, const Identifier& i) override {
        if (tree.hasType(IDs::PROCESSOR)) {
            if (i == IDs::bypassed) {
                if (auto node = getNodeForState(tree)) {
                    node->setBypassed(tree[IDs::bypassed]);
                }
            } else if (i == IDs::allowDefaultConnections) {
                updateDefaultConnectionsForProcessor(tree, true);
            }
        } else if (i == IDs::focusedProcessorSlot) { // TODO change to focusedTrackIndex, and remember to reset inputs also when a processor is added to the focused track
            if (!isDeleting)
                resetDefaultExternalInputs(nullptr);
        }
    }

    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (!tracksManager.isCurrentlyDraggingProcessor()) {
                if (getProcessorWrapperForState(child) == nullptr) {
                    addProcessor(child);
                }
            }
        } else if (child.hasType(IDs::CONNECTION)) {
            if (!tracksManager.isCurrentlyDraggingProcessor()) {
                const ValueTree &sourceState = child.getChildWithName(IDs::SOURCE);
                const ValueTree &destState = child.getChildWithName(IDs::DESTINATION);

                if (auto *source = getNodeForState(sourceState)) {
                    if (auto *dest = getNodeForState(destState)) {
                        int destChannel = destState[IDs::channel];
                        int sourceChannel = sourceState[IDs::channel];

                        source->outputs.add({dest, destChannel, sourceChannel});
                        dest->inputs.add({source, sourceChannel, destChannel});
                        topologyChanged();
                    }
                }
                if (child.hasProperty(IDs::isCustomConnection)) {
                    const auto& processor = getProcessorStateForNodeId(getNodeIdForState(sourceState));
                    jassert(processor.isValid());
                    updateDefaultConnectionsForProcessor(processor, true);
                }
            }
        } else if (child.hasType(IDs::TRACK)) {
            recursivelyInitializeState(child);
        } else if (child.hasType(IDs::CHANNEL)) {
            updateIoChannelEnabled(parent, child, true);
            removeIllegalConnections();
        }
    }

    void valueTreeChildRemoved(ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (!tracksManager.isCurrentlyDraggingProcessor()) {
                if (!isMoving) {
                    removeProcessor(child);
                }
            }
            for (int i = activePluginWindows.size(); --i >= 0;) {
                if (!nodes.contains(activePluginWindows.getUnchecked(i)->node)) {
                    activePluginWindows.remove(i);
                }
            }
        } else if (child.hasType(IDs::CONNECTION)) {
            if (!tracksManager.isCurrentlyDraggingProcessor()) {
                const ValueTree &sourceState = child.getChildWithName(IDs::SOURCE);
                const ValueTree &destState = child.getChildWithName(IDs::DESTINATION);

                if (auto *source = getNodeForState(sourceState)) {
                    if (auto *dest = getNodeForState(destState)) {
                        int destChannel = destState[IDs::channel];
                        int sourceChannel = sourceState[IDs::channel];

                        source->outputs.removeAllInstancesOf({dest, destChannel, sourceChannel});
                        dest->inputs.removeAllInstancesOf({source, sourceChannel, destChannel});
                        topologyChanged();
                    }
                }
            }
        } else if (child.hasType(IDs::CHANNEL)) {
            updateIoChannelEnabled(parent, child, false);
            removeIllegalConnections();
        }
    }

    void valueTreeChildOrderChanged(ValueTree& parent, int oldIndex, int newIndex) override {}

    void valueTreeParentChanged(ValueTree& treeWhoseParentHasChanged) override {}

    void valueTreeRedirected(ValueTree& treeWhichHasBeenChanged) override {}

    void valueTreeChildWillBeMovedToNewParent(ValueTree child, ValueTree& oldParent, int oldIndex, ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR))
            isMoving = true;
    }

    void valueTreeChildHasMovedToNewParent(ValueTree child, ValueTree& oldParent, int oldIndex, ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR))
            isMoving = false;
    }

    /*
     * The following ProcessorLifecycle methods take care of things that could be done
     * inside of the ValueTreeChanged methods above. They are here because they
     * create further undoable actions as side-effects and we must avoid recursive
     * undoable actions.
     *
     * This way, the ValueTreeChanged methods in this class only update the AudioGraph
     * and UI state.
     */
    void processorCreated(const ValueTree& processor) override {
        updateAllDefaultConnections();
    };

    void processorWillBeDestroyed(const ValueTree& processor) override {
        isDeleting = true;
        resetDefaultExternalInputs(getDragDependentUndoManager(), false);
        ValueTree(processor).removeProperty(IDs::processorInitialized, nullptr);
        disconnectNode(getNodeIdForState(processor));
    };

    void processorHasBeenDestroyed(const ValueTree& processor) override {
        updateAllDefaultConnections();
        isDeleting = false;
    };

    void updateIoChannelEnabled(const ValueTree& parent, const ValueTree& channel, bool enabled) {
        String processorName = parent.getParent()[IDs::name];
        bool isInput;
        if (processorName == "Audio Input" && parent.hasType(IDs::OUTPUT_CHANNELS))
            isInput = true;
        else if (processorName == "Audio Output" && parent.hasType(IDs::INPUT_CHANNELS))
            isInput = false;
        else
            return;
        if (auto* audioDevice = deviceManager.getCurrentAudioDevice()) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            auto &channels = isInput ? config.inputChannels : config.outputChannels;
            const auto &channelNames = isInput ? audioDevice->getInputChannelNames() : audioDevice->getOutputChannelNames();
            auto channelIndex = channelNames.indexOf(channel[IDs::name].toString());
            if (channelIndex != -1 && ((enabled && !channels[channelIndex]) || (!enabled && channels[channelIndex]))) {
                channels.setBit(channelIndex, enabled);
                deviceManager.setAudioDeviceSetup(config, true);
            }
        }
    }

    void timerCallback() override {
        bool anythingUpdated = false;

        for (auto& nodeIdAndprocessorWrapper : processorWrapperForNodeId) {
            if (nodeIdAndprocessorWrapper.second->flushParameterValuesToValueTree())
                anythingUpdated = true;
        }

        startTimer(anythingUpdated ? 1000 / 50 : jlimit(50, 500, getTimerInterval() + 20));
    }
};
