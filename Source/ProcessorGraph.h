#pragma once

#include <Identifiers.h>
#include <view/PluginWindow.h>
#include <processors/StatefulAudioProcessorWrapper.h>

#include "JuceHeader.h"
#include "processors/ProcessorManager.h"
#include "Project.h"
#include "StatefulAudioProcessorContainer.h"
#include "push2/Push2MidiCommunicator.h"

class ProcessorGraph : public AudioProcessorGraph, public StatefulAudioProcessorContainer, private ValueTree::Listener,
                       private ProjectChangeListener, private Timer {
public:
    explicit ProcessorGraph(Project &project, UndoManager &undoManager, AudioDeviceManager& deviceManager, Push2MidiCommunicator& push2MidiCommunicator)
            : undoManager(undoManager), project(project), deviceManager(deviceManager), push2MidiCommunicator(push2MidiCommunicator) {
        enableAllBuses();

        project.getState().addListener(this);
        this->project.addProjectChangeListener(this);
        project.setStatefulAudioProcessorContainer(this);
    }

    ~ProcessorGraph() override {
        project.setStatefulAudioProcessorContainer(nullptr);
        this->project.removeProjectChangeListener(this);
        project.getState().removeListener(this);
    }

    StatefulAudioProcessorWrapper* getProcessorWrapperForNodeId(NodeID nodeId) const override {
        if (nodeId == NA_NODE_ID)
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
        const ValueTree &gain = project.getMixerChannelProcessorForTrack(project.getMasterTrack());
        return getProcessorWrapperForState(gain);
    }

    ResizableWindow* getOrCreateWindowFor(AudioProcessorGraph::Node* node, PluginWindow::Type type) {
        jassert(node != nullptr);

        for (auto* w : activePluginWindows)
            if (w->node == node && w->type == type)
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

    void beginDraggingNode(NodeID nodeId, const Point<int> &trackAndSlot) {
        if (auto* processorWrapper = getProcessorWrapperForNodeId(nodeId)) {
            if (processorWrapper->processor->getName() == MixerChannelProcessor::name())
                // mixer channel processors are special processors.
                // they could be dragged and reconnected like any old processor, but please don't :)
                return;
            currentlyDraggingNodeId = nodeId;
            currentlyDraggingTrackAndSlot = trackAndSlot;
            initialDraggingTrackAndSlot = currentlyDraggingTrackAndSlot;
            project.makeConnectionsSnapshot();
        }
    }

    void setNodePosition(NodeID nodeId, const Point<int> &trackAndSlot) {
        if (currentlyDraggingNodeId == NA_NODE_ID)
            return;
        auto processor = getProcessorStateForNodeId(nodeId);
        if (processor.isValid()) {
            if (currentlyDraggingTrackAndSlot != trackAndSlot && trackAndSlot.y < project.maxSlotForTrack(project.getTrack(trackAndSlot.x))) {
                currentlyDraggingTrackAndSlot = trackAndSlot;

                moveProcessor(processor, trackAndSlot.x, trackAndSlot.y);
                if (currentlyDraggingTrackAndSlot == initialDraggingTrackAndSlot) {
                    project.restoreConnectionsSnapshot();
                }
            }
        }
    }

    void endDraggingNode(NodeID nodeId) {
        if (currentlyDraggingNodeId != NA_NODE_ID && currentlyDraggingTrackAndSlot != initialDraggingTrackAndSlot) {
            // update the audio graph to match the current preview UI graph.
            auto processor = getProcessorStateForNodeId(nodeId);
            jassert(processor.isValid());
            moveProcessor(processor, initialDraggingTrackAndSlot.x, initialDraggingTrackAndSlot.y);
            project.restoreConnectionsSnapshot();
            currentlyDraggingNodeId = NA_NODE_ID;
            moveProcessor(processor, currentlyDraggingTrackAndSlot.x, currentlyDraggingTrackAndSlot.y);
            if (project.isInShiftMode()) {
                project.setShiftMode(false);
                updateAllDefaultConnections(true);
                project.setShiftMode(true);
            }
        }
        currentlyDraggingNodeId = NA_NODE_ID;
    }

    void moveProcessor(ValueTree &processorState, int toTrackIndex, int toSlot) {
        const ValueTree &toTrack = project.getTrack(toTrackIndex);
        int fromSlot = processorState[IDs::processorSlot];
        if (fromSlot == toSlot && processorState.getParent() == toTrack)
            return;

        project.setProcessorSlot(processorState.getParent(), processorState, toSlot, getDragDependentUndoManager());

        const int insertIndex = project.getParentIndexForProcessor(toTrack, processorState, getDragDependentUndoManager());
        Helpers::moveSingleItem(processorState, toTrack, insertIndex, getDragDependentUndoManager());

        project.makeSlotsValid(toTrack, getDragDependentUndoManager());
        updateAllDefaultConnections();
    }

    bool canConnectUi(const Connection& c) const {
        if (auto* source = getNodeForId(c.source.nodeID))
            if (auto* dest = getNodeForId(c.destination.nodeID))
                return canConnectUi(source, c.source.channelIndex,
                                   dest, c.destination.channelIndex);

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

        return !project.getConnectionMatching({{source->nodeID, sourceChannel}, {dest->nodeID, destChannel}}).isValid();
    }

    bool addConnection(const Connection& c) override {
        if (canConnectUi(c)) {
            disconnectDefaultOutgoing(c.source.nodeID, c.source.isMIDI() ? midi : audio);
            project.addConnection(c, getDragDependentUndoManager(), false);
            return true;
        }
        return false;
    }
    
    bool removeConnection(const Connection& c) override {
        const ValueTree &connectionState = project.getConnectionMatching(c);
        bool removed = project.removeConnection(c, getDragDependentUndoManager(), true, true);
        if (removed && connectionState.hasProperty(IDs::isCustomConnection)) {
            const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto nodeId = getNodeIdForState(sourceState);
            const auto& processor = getProcessorStateForNodeId(nodeId);
            jassert(processor.isValid());
            updateDefaultConnectionsForProcessor(processor, true);
        }
        return removed;
    }

    bool addDefaultConnection(const Connection& c) {
        if (canConnectUi(c)) {
            project.addConnection(c, getDragDependentUndoManager());
            return true;
        }
        return false;
    }

    bool addCustomConnection(const Connection& c) {
        if (canConnectUi(c)) {
            project.addConnection(c, &undoManager, false);
            return true;
        }
        return false;
    }

    bool removeDefaultConnection(const Connection& c) {
        return project.removeConnection(c, getDragDependentUndoManager(), true, false);
    }

    void setDefaultConnectionsAllowed(NodeID nodeId, bool defaultConnectionsAllowed) {
        auto processor = getProcessorStateForNodeId(nodeId);
        jassert(processor.isValid());
        processor.setProperty(IDs::allowDefaultConnections, defaultConnectionsAllowed, &undoManager);
    }

    bool disconnectNode(NodeID nodeId) override {
        return doDisconnectNode(nodeId, all, true, true, true, true);
    }

    bool disconnectDefaults(NodeID nodeId) {
        return doDisconnectNode(nodeId, all, true, false, true, true);
    }

    bool disconnectDefaultOutgoing(NodeID nodeId, ConnectionType connectionType) {
        return doDisconnectNode(nodeId, connectionType, true, false, false, true);
    }

    bool disconnectDefaultIncoming(NodeID nodeId, ConnectionType connectionType) {
        return doDisconnectNode(nodeId, connectionType, true, false, true, false);
    }

    bool disconnectCustom(NodeID nodeId) {
        return doDisconnectNode(nodeId, all, false, true, true, true);
    }

    bool removeNode(NodeID nodeId) override  {
        auto processor = getProcessorStateForNodeId(nodeId);
        if (processor.isValid()) {
            disconnectCustom(nodeId);
            processor.getParent().removeChild(processor, &undoManager);
            updateAllDefaultConnections();
            return true;
        }
        return false;
    }

    UndoManager &undoManager;
private:
    NodeID currentlyDraggingNodeId = NA_NODE_ID;
    Point<int> currentlyDraggingTrackAndSlot;
    Point<int> initialDraggingTrackAndSlot;

    std::unordered_map<NodeID, std::unique_ptr<StatefulAudioProcessorWrapper> > processorWrapperForNodeId;

    Project &project;
    AudioDeviceManager &deviceManager;
    Push2MidiCommunicator &push2MidiCommunicator;
    
    OwnedArray<PluginWindow> activePluginWindows;

    Array<int> defaultAudioConnectionChannels {0, 1};
    Array<int> defaultMidiConnectionChannels {midiChannelIndex};

    bool isMoving { false };

    ValueTree lastSelectedProcessor {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    inline UndoManager* getDragDependentUndoManager() {
        return currentlyDraggingNodeId == NA_NODE_ID ? &undoManager : nullptr;
    }

    bool doDisconnectNode(NodeID nodeId, ConnectionType connectionType, bool defaults, bool custom, bool incoming, bool outgoing) {
        const auto connections = project.getConnectionsForNode(nodeId, connectionType, incoming, outgoing);
        bool anyRemoved = false;
        for (const auto &connection : connections) {
            if (project.removeConnection(connection, getDragDependentUndoManager(), defaults, custom))
                anyRemoved = true;
        }

        return anyRemoved;
    }

    void addProcessor(const ValueTree &processorState) {
        static String errorMessage = "Could not create processor";
        PluginDescription *desc = project.getTypeForIdentifier(processorState[IDs::id]);
        auto *processor = project.getFormatManager().createPluginInstance(*desc, getSampleRate(), getBlockSize(), errorMessage);
        if (processorState.hasProperty(IDs::state)) {
            MemoryBlock memoryBlock;
            memoryBlock.fromBase64Encoding(processorState[IDs::state].toString());
            processor->setStateInformation(memoryBlock.getData(), (int) memoryBlock.getSize());
        }

        const Node::Ptr &newNode = processorState.hasProperty(IDs::nodeId) ?
                                   addNode(processor, getNodeIdForState(processorState)) :
                                   addNode(processor);
        processorWrapperForNodeId[newNode->nodeID] = std::make_unique<StatefulAudioProcessorWrapper>
                (processor, newNode->nodeID, processorState, undoManager, project.getDeviceManager());
        if (processorWrapperForNodeId.size() == 1)
            // Added the first processor. Start the timer that flushes new processor state to their value trees.
            startTimerHz(10);

        if (auto *midiInputProcessor = dynamic_cast<MidiInputProcessor *>(processor)) {
            const String &deviceName = processorState.getProperty(IDs::deviceName);
            midiInputProcessor->setDeviceName(deviceName);
            if (deviceName.containsIgnoreCase(push2MidiDeviceName)) {
                push2MidiCommunicator.addMidiInputCallback(&midiInputProcessor->getMidiMessageCollector());
            } else {
                deviceManager.addMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
            }
        } else if (auto *midiOutputProcessor = dynamic_cast<MidiOutputProcessor *>(processor)) {
            const String &deviceName = processorState.getProperty(IDs::deviceName);
            if (auto* enabledMidiOutput = deviceManager.getEnabledMidiOutput(deviceName))
                midiOutputProcessor->setMidiOutput(enabledMidiOutput);
        }
    }

    void removeProcessor(const ValueTree &processor) {
        auto* processorWrapper = getProcessorWrapperForState(processor);
        jassert(processorWrapper);
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
        processorWrapperForNodeId.erase(getNodeIdForState(processor));
        nodes.removeObject(getNodeForId(getNodeIdForState(processor)));
        topologyChanged();
    }

    void resetDefaultExternalInputs(const ValueTree &selectedProcessor) {
        auto audioInputNodeId = getNodeIdForState(project.getAudioInputProcessorState());
        disconnectDefaultOutgoing(audioInputNodeId, audio);
        if (selectedProcessor.isValid())
            addDefaultExternalInputConnections(selectedProcessor, audioInputNodeId, audio);
        for (const auto& midiInputProcessor : project.getInput()) {
            if (midiInputProcessor.getProperty(IDs::name) == MidiInputProcessor::name()) {
                auto nodeId = getNodeIdForState(midiInputProcessor);
                disconnectDefaultOutgoing(nodeId, midi);
                if (selectedProcessor.isValid())
                    addDefaultExternalInputConnections(selectedProcessor, nodeId, midi);
            }
        }
    }

    // Find the upper-right-most processor that flows into the selected processor
    void addDefaultExternalInputConnections(const ValueTree &selectedProcessor,
                                            NodeID externalSourceNodeId, ConnectionType connectionType) {
        if (project.getNumTracks() == 0)
            return;

        int lowestSlot = INT_MAX;
        NodeID upperRightMostProcessorNodeId = NA_NODE_ID;
        for (int i = project.getNumTracks() - 1; i >= 0; i--) {
            const auto& track = project.getTrack(i);
            if (track.getNumChildren() == 0)
                continue;

            const auto& firstProcessor = track.getChild(0);
            auto firstProcessorNodeId = getNodeIdForState(firstProcessor);
            int slot = firstProcessor[IDs::processorSlot];
            if (slot < lowestSlot &&
                areProcessorsConnected(firstProcessorNodeId, getNodeIdForState(selectedProcessor), all) &&
                !project.hasIncomingConnections(firstProcessorNodeId, connectionType)) {

                lowestSlot = firstProcessor[IDs::processorSlot];
                upperRightMostProcessorNodeId = firstProcessorNodeId;
            }
        }

        if (upperRightMostProcessorNodeId != NA_NODE_ID) {
            const auto &defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
            for (auto channel : defaultConnectionChannels) {
                addDefaultConnection({{externalSourceNodeId, channel}, {upperRightMostProcessorNodeId, channel}});
            }
        }
    }

    bool areProcessorsConnected(NodeID upstreamNodeId, NodeID downstreamNodeId, ConnectionType connectionType) const {
        if (upstreamNodeId == downstreamNodeId)
            return true;

        auto outgoingConnections = project.getConnectionsForNode(upstreamNodeId, connectionType, false, true);
        for (const auto& outgoingConnection : outgoingConnections) {
            auto otherDownstreamNodeId = getNodeIdForState(outgoingConnection.getChildWithName(IDs::DESTINATION));
            if (downstreamNodeId == otherDownstreamNodeId ||
                areProcessorsConnected(otherDownstreamNodeId, downstreamNodeId, connectionType))
                return true;
        }
        return false;
    }

    inline const Array<int>& getDefaultConnectionChannels(ConnectionType connectionType) const {
        return connectionType == audio ? defaultAudioConnectionChannels : defaultMidiConnectionChannels;
    }

    const ValueTree findProcessorToFlowInto(const ValueTree &parent, const ValueTree &processor,
                                            ConnectionType connectionType, const ValueTree &excluding = {}) {
        if (!processor.hasType(IDs::PROCESSOR) || !isProcessorAProducer(processor, connectionType))
            return {};

        // If a non-effect (another producer) is under this processor in the same track, and no effect processors
        // to the right have a lower slot, choose to block this producer's output by the other producer,
        // effectively replacing it.
        ValueTree fallbackBlockingProcessor;

        int siblingDelta = 0;
        ValueTree otherParent;
        while ((otherParent = parent.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : otherParent) {
                if (otherProcessor == processor || otherProcessor == excluding) continue;
                bool isBelow = int(otherProcessor[IDs::processorSlot]) > int(processor[IDs::processorSlot]) ||
                               (otherParent.hasProperty(IDs::isMasterTrack) && parent != otherParent);
                if (!isBelow) continue;
                if (canProcessorDefaultConnectTo(parent, processor, otherParent, otherProcessor, connectionType) &&
                    (!fallbackBlockingProcessor.isValid() || int(otherProcessor[IDs::processorSlot]) <= int(fallbackBlockingProcessor[IDs::processorSlot]))) {
                    return otherProcessor;
                } else if (parent == otherParent) {
                    fallbackBlockingProcessor = otherProcessor;
                    break;
                }
            }
        }

        if (fallbackBlockingProcessor.isValid())
            return fallbackBlockingProcessor;

        return project.getAudioOutputProcessorState();
    }

    inline bool isProcessorAProducer(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumOutputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->producesMidi());
        }
        return false;
    }

    inline bool isProcessorAnEffect(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumInputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->acceptsMidi());
        }
        return false;
    }

    inline bool canProcessorDefaultConnectTo(const ValueTree &parent, const ValueTree &processor,
                                                ValueTree &otherParent, const ValueTree &otherProcessor,
                                                ConnectionType connectionType) const {
        if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor)
            return false;

        return isProcessorAProducer(processor, connectionType) && isProcessorAnEffect(otherProcessor, connectionType);
    }

    const ValueTree getFirstMasterTrackProcessorWithInputs(ConnectionType connectionType, const ValueTree& excluding={}) const {
        for (const auto& processor : project.getMasterTrack()) {
            if (processor.hasType(IDs::PROCESSOR) && processor != excluding && isProcessorAnEffect(processor, connectionType)) {
                return processor;
            }
        }
        return {};
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
        for (auto track : project.getTracks()) {
            for (auto processor : track) {
                updateDefaultConnectionsForProcessor(processor, false, makeInvalidDefaultsIntoCustom);
            }
        }
        resetDefaultExternalInputs(project.getSelectedProcessor());
    }
    
    void updateDefaultConnectionsForProcessor(const ValueTree &processor, bool updateExternalInputs, bool makeInvalidDefaultsIntoCustom=false) {
        for (auto connectionType : {audio, midi}) {
            NodeID nodeId = getNodeIdForState(processor);
            if (!processor[IDs::allowDefaultConnections]) {
                disconnectDefaults(nodeId);
                return;
            }
            auto outgoingCustomConnections = project.getConnectionsForNode(nodeId, connectionType, false, true, true, false);
            if (!outgoingCustomConnections.isEmpty()) {
                disconnectDefaultOutgoing(nodeId, connectionType);
                return;
            }
            auto outgoingDefaultConnections = project.getConnectionsForNode(nodeId, connectionType, false, true, false, true);
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
                        addDefaultConnection({{nodeId, channel}, {nodeIdToConnectTo, channel}});
                    }
                }
            }
        }
        if (updateExternalInputs) {
            resetDefaultExternalInputs(project.getSelectedProcessor());
        }
    }
    
    void valueTreePropertyChanged(ValueTree& tree, const Identifier& i) override {
        if (tree.hasType(IDs::PROCESSOR)) {
            if (i == IDs::bypassed) {
                if (auto node = getNodeForState(tree)) {
                    node->setBypassed(tree[IDs::bypassed]);
                }
            } else if (i == IDs::selected && tree[IDs::selected] && tree != lastSelectedProcessor) {
                lastSelectedProcessor = tree;
                resetDefaultExternalInputs(tree);
            } else if (i == IDs::allowDefaultConnections) {
                updateDefaultConnectionsForProcessor(tree, true);
            }
        }
    }

    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (currentlyDraggingNodeId == NA_NODE_ID) {
                if (getProcessorWrapperForState(child) == nullptr) {
                    addProcessor(child);
                }
                ValueTree mutableProcessor = child;
                if (child.hasProperty(IDs::processorInitialized))
                    mutableProcessor.sendPropertyChangeMessage(IDs::processorInitialized);
                else
                    mutableProcessor.setProperty(IDs::processorInitialized, true, nullptr);

                if (child[IDs::selected]) {
                    lastSelectedProcessor = child;
                    child.sendPropertyChangeMessage(IDs::selected);
                }
            }
        } else if (child.hasType(IDs::CONNECTION)) {
            if (currentlyDraggingNodeId == NA_NODE_ID) {
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
            if (currentlyDraggingNodeId == NA_NODE_ID) {
                if (!isMoving) {
                    removeProcessor(child);
                }
            }
            for (int i = activePluginWindows.size(); --i >= 0;) {
                if (!nodes.contains(activePluginWindows.getUnchecked(i)->node)) {
                    activePluginWindows.remove(i);
                }
            }
            if (child == lastSelectedProcessor)
                lastSelectedProcessor = {};
        } else if (child.hasType(IDs::CONNECTION)) {
            if (currentlyDraggingNodeId == NA_NODE_ID) {
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

    void valueTreeChildWillBeMovedToNewParent(ValueTree child, const ValueTree& oldParent, int oldIndex, const ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR))
            isMoving = true;
    }

    void valueTreeChildHasMovedToNewParent(ValueTree child, const ValueTree& oldParent, int oldIndex, const ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR))
            isMoving = false;
    }

    /*
     * The following ProjectChanged methods take care of things that could be done
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
        ValueTree(processor).removeProperty(IDs::processorInitialized, nullptr);
        disconnectNode(getNodeIdForState(processor));
    };

    void processorHasBeenDestroyed(const ValueTree& processor) override {
        updateAllDefaultConnections();
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
