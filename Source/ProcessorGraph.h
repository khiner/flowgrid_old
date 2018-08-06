#pragma once

#include <Identifiers.h>
#include <view/PluginWindow.h>
#include <processors/StatefulAudioProcessorWrapper.h>

#include "JuceHeader.h"
#include "processors/ProcessorManager.h"
#include "Project.h"

class ProcessorGraph : public AudioProcessorGraph, private ValueTree::Listener, private ProjectChangeListener {
public:
    explicit ProcessorGraph(Project &project, UndoManager &undoManager, AudioDeviceManager& deviceManager)
            : undoManager(undoManager), project(project), deviceManager(deviceManager) {
        enableAllBuses();

        project.getState().addListener(this);
        addProcessor(project.getAudioInputProcessorState());
        addProcessor(project.getAudioOutputProcessorState());
        this->project.addProjectChangeListener(this);
    }

    void audioDeviceManagerInitialized() {
        for (const auto& inputProcessor : project.getInput())
            if (inputProcessor.getProperty(IDs::name) == MidiInputProcessor::name())
                addProcessor(inputProcessor);
        for (const auto& outputProcessor : project.getOutput())
            if (outputProcessor.getProperty(IDs::name) == MidiOutputProcessor::name())
                addProcessor(outputProcessor);
    }
    
    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return getProcessorWrapperForNodeId(getNodeIdForState(processorState));
    }

    static const NodeID getNodeIdForState(const ValueTree &processorState) {
        return processorState.isValid() ? NodeID(int(processorState[IDs::nodeId])) : NA_NODE_ID;
    }

    Node* getNodeForState(const ValueTree &processorState) const {
        return getNodeForId(getNodeIdForState(processorState));
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(NodeID nodeId) const {
        if (nodeId == NA_NODE_ID)
            return nullptr;

        if (auto* node = getNodeForId(nodeId)) {
            AudioProcessor *processor = node->getProcessor();
            for (auto *processorWrapper : processerWrappers) {
                if (processorWrapper->processor == processor)
                    return processorWrapper;
            }
        }
        return nullptr;
    }

    StatefulAudioProcessorWrapper *getMasterGainProcessor() {
        const ValueTree &gain = project.getMixerChannelProcessorForTrack(project.getMasterTrack());
        return getProcessorWrapperForState(gain);
    }

    PluginWindow* getOrCreateWindowFor(AudioProcessorGraph::Node* node, PluginWindow::Type type) {
        jassert(node != nullptr);

        for (auto* w : activePluginWindows)
            if (w->node == node && w->type == type)
                return w;

        if (auto* processor = node->getProcessor()) {
            if (auto* plugin = dynamic_cast<AudioPluginInstance*>(processor)) {
                auto description = plugin->getPluginDescription();
            }

            return activePluginWindows.add(new PluginWindow(node, type, activePluginWindows));
        }

        return nullptr;
    }

    bool closeAnyOpenPluginWindows() {
        bool wasEmpty = activePluginWindows.isEmpty();
        activePluginWindows.clear();
        return !wasEmpty;
    }
    
    void beginDraggingNode(NodeID nodeId, const Point<int> &trackAndSlot) {
        if (auto* processorWrapper = getProcessorWrapperForNodeId(nodeId)) {
            project.setSelectedProcessor(processorWrapper->state);

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
        if (auto *processor = getProcessorWrapperForNodeId(nodeId)) {
            if (currentlyDraggingTrackAndSlot != trackAndSlot) {
                currentlyDraggingTrackAndSlot = trackAndSlot;

                moveProcessor(processor->state, initialDraggingTrackAndSlot.x, initialDraggingTrackAndSlot.y);
                project.restoreConnectionsSnapshot();
                if (currentlyDraggingTrackAndSlot != initialDraggingTrackAndSlot) {
                    moveProcessor(processor->state, trackAndSlot.x, trackAndSlot.y);
                }
            }
        }
    }

    void endDraggingNode(NodeID nodeId) {
        if (currentlyDraggingNodeId != NA_NODE_ID && currentlyDraggingTrackAndSlot != initialDraggingTrackAndSlot) {
            // update the audio graph to match the current preview UI graph.
            StatefulAudioProcessorWrapper *processor = getProcessorWrapperForNodeId(nodeId);
            moveProcessor(processor->state, initialDraggingTrackAndSlot.x, initialDraggingTrackAndSlot.y);
            project.restoreConnectionsSnapshot();
            currentlyDraggingNodeId = NA_NODE_ID;
            moveProcessor(processor->state, currentlyDraggingTrackAndSlot.x, currentlyDraggingTrackAndSlot.y);
        }
        currentlyDraggingNodeId = NA_NODE_ID;
    }

    void moveProcessor(ValueTree &processorState, int toTrackIndex, int toSlot) {
        const ValueTree &toTrack = project.getTrack(toTrackIndex);
        int fromSlot = processorState[IDs::processorSlot];
        if (fromSlot == toSlot && processorState.getParent() == toTrack)
            return;

        removeDefaultConnections(processorState);

        processorState.setProperty(IDs::processorSlot, toSlot, getDragDependentUndoManager());
        const int insertIndex = project.getParentIndexForProcessor(toTrack, processorState, getDragDependentUndoManager());
        Helpers::moveSingleItem(processorState, toTrack, insertIndex, getDragDependentUndoManager());

        addDefaultConnections(processorState);
        project.makeSlotsValid(toTrack, getDragDependentUndoManager());
        resetDefaultExternalInputConnections();
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
            project.addConnection(c, getDragDependentUndoManager(), false);
            return true;
        }
        return false;
    }

    bool removeConnection(const Connection& c) override {
        return project.removeConnection(c, getDragDependentUndoManager(), true, true);
    }

    bool addDefaultConnection(const Connection& c) {
        if (canConnectUi(c)) {
            project.addConnection(c, getDragDependentUndoManager());
            return true;
        }
        return false;
    }

    bool removeDefaultConnection(const Connection& c) {
        return project.removeConnection(c, getDragDependentUndoManager(), true, false);
    }

    enum ConnectionType { audio, midi, all };

    void connectDefaults(NodeID nodeId) {
        addDefaultConnections(getProcessorWrapperForNodeId(nodeId)->state);
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

    bool disconnectDefaultIncoming(NodeID nodeId) {
        return doDisconnectNode(nodeId, all, true, false, true, false);
    }

    bool disconnectCustom(NodeID nodeId) {
        return doDisconnectNode(nodeId, all, false, true, true, true);
    }

    bool removeNode(NodeID nodeId) override  {
        if (auto* processorWrapper = getProcessorWrapperForNodeId(nodeId)) {
            removeDefaultConnections(processorWrapper->state);
            disconnectCustom(nodeId);
            if (auto *midiInputProcessor = dynamic_cast<MidiInputProcessor *>(processorWrapper->processor)) {
                const String &deviceName = processorWrapper->state.getProperty(IDs::deviceName);
                deviceManager.removeMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
            }
            processorWrapper->state.getParent().removeChild(processorWrapper->state, &undoManager);
            return true;
        }
        return false;
    }

    const static NodeID NA_NODE_ID = 0;
    UndoManager &undoManager;
private:

    NodeID currentlyDraggingNodeId = NA_NODE_ID;
    Point<int> currentlyDraggingTrackAndSlot;
    Point<int> initialDraggingTrackAndSlot;

    Project &project;
    AudioDeviceManager &deviceManager;
    
    OwnedArray<StatefulAudioProcessorWrapper> processerWrappers;
    OwnedArray<PluginWindow> activePluginWindows;

    Array<int> defaultAudioConnectionChannels {0, 1};
    Array<int> defaultMidiConnectionChannels {midiChannelIndex};

    bool isMoving { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    inline UndoManager* getDragDependentUndoManager() {
        return currentlyDraggingNodeId == NA_NODE_ID ? &undoManager : nullptr;
    }

    bool doDisconnectNode(NodeID nodeId, ConnectionType connectionType, bool defaults, bool custom, bool incoming, bool outgoing) {
        const Array<ValueTree> connections = project.getConnectionsForNode(nodeId, connectionType == audio || connectionType == all, connectionType == midi || connectionType == all, incoming, outgoing);
        bool anyRemoved = false;
        for (const auto &connection : connections)
            if (project.removeConnection(connection, getDragDependentUndoManager(), defaults, custom)) {
                anyRemoved = true;
            }

        return anyRemoved;
    }

    void addProcessor(const ValueTree &processorState) {
        static String errorMessage = "Could not create processor";
        PluginDescription *desc = project.getTypeForIdentifier(processorState[IDs::id]);
        auto *processor = project.getFormatManager().createPluginInstance(*desc, getSampleRate(), getBlockSize(), errorMessage);

        const Node::Ptr &newNode = processorState.hasProperty(IDs::nodeId) ?
                                   addNode(processor, getNodeIdForState(processorState)) :
                                   addNode(processor);
        auto *processorWrapper = new StatefulAudioProcessorWrapper(processor, newNode->nodeID, processorState, project, undoManager);
        processerWrappers.add(processorWrapper);

        if (auto *midiInputProcessor = dynamic_cast<MidiInputProcessor *>(processor)) {
            const String &deviceName = processorState.getProperty(IDs::deviceName);
            midiInputProcessor->setDeviceName(deviceName);
            deviceManager.addMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
        } else if (auto *midiOutputProcessor = dynamic_cast<MidiOutputProcessor *>(processor)) {
            const String &deviceName = processorState.getProperty(IDs::deviceName);
            if (auto* enabledMidiOutput = deviceManager.getEnabledMidiOutput(deviceName))
                midiOutputProcessor->setMidiOutput(enabledMidiOutput);
        }
        const Array<ValueTree> &nodeConnections = project.getConnectionsForNode(newNode->nodeID);
        for (auto& connection : nodeConnections) {
            valueTreeChildAdded(project.getConnections(), connection);
        }
    }

    struct NeighborNodes {
        Array<NodeID> before {};
        NodeID after = NA_NODE_ID;
    };

    void resetDefaultExternalInputConnections() {
        auto audioInputNodeId = getNodeIdForState(project.getAudioInputProcessorState());
        resetDefaultExternalInputConnections(audioInputNodeId, audio);
        for (const auto& midiInputProcessor : project.getInput())
            if (midiInputProcessor.getProperty(IDs::name) == MidiInputProcessor::name())
                resetDefaultExternalInputConnections(getNodeIdForState(midiInputProcessor), midi);
    }
    
    void resetDefaultExternalInputConnections(NodeID externalSourceNodeId, ConnectionType connectionType) {
        for (const auto& track : project.getTracks()) {
            if (track.hasType(IDs::MASTER_TRACK))
                continue;
            const auto &defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
            for (const auto& child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    auto nodeId = getNodeIdForState(child);
                    for (auto channel : defaultConnectionChannels) {
                        removeDefaultConnection({{externalSourceNodeId, channel}, {nodeId, channel}});
                    }
                }
            }
            const auto& firstChild = track.getChildWithName(IDs::PROCESSOR);
            if (firstChild.isValid() && isProcessorAnEffect(firstChild, connectionType)) {
                auto nodeId = getNodeIdForState(firstChild);
                for (auto channel : defaultConnectionChannels) {
                    addDefaultConnection({{externalSourceNodeId, channel}, {nodeId, channel}});
                }
            }
        }
    }

    void addDefaultConnections(const ValueTree &processor) {
        addDefaultConnections(processor, audio);
        addDefaultConnections(processor, midi);
    }

    void removeDefaultConnections(const ValueTree &processor) {
        removeDefaultConnections(processor, audio);
        removeDefaultConnections(processor, midi);
    }

    void addDefaultConnections(const ValueTree &processor, ConnectionType connectionType) {
        const auto& defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
        const NeighborNodes &neighbors = findNeighborProcessors(processor, connectionType);
        auto nodeId = getNodeIdForState(processor);
        for (auto before : neighbors.before) {
            disconnectDefaultOutgoing(before, connectionType);
            for (auto channel : defaultConnectionChannels) {
                addDefaultConnection({{before, channel}, {nodeId, channel}});
            }
        }
        if (neighbors.after != NA_NODE_ID) {
            for (auto channel : defaultConnectionChannels) {
                addDefaultConnection({{nodeId, channel}, {neighbors.after, channel}});
            }
        }
    }

    void removeDefaultConnections(const ValueTree &processor, ConnectionType connectionType) {
        const auto& defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
        const NeighborNodes &neighbors = findNeighborProcessors(processor, connectionType);
        auto nodeId = getNodeIdForState(processor);
        if (neighbors.after != NA_NODE_ID) {
            for (auto channel : defaultConnectionChannels) {
                removeDefaultConnection({{nodeId, channel}, {neighbors.after, channel}});
            }
        }
        for (auto before : neighbors.before) {
            for (auto channel : defaultConnectionChannels) {
                removeDefaultConnection({{before, channel}, {nodeId, channel}});
            }
            const NeighborNodes &neighborsForBeforeNode = findNeighborProcessors(getProcessorWrapperForNodeId(before)->state, connectionType, processor, false, true);
            if (neighborsForBeforeNode.after != NA_NODE_ID) {
                for (auto channel : defaultConnectionChannels) {
                    addDefaultConnection({{before, channel}, {neighborsForBeforeNode.after, channel}});
                }
            }
        }
    }

    inline const Array<int>& getDefaultConnectionChannels(ConnectionType connectionType) const {
        return connectionType == audio ? defaultAudioConnectionChannels : defaultMidiConnectionChannels;
    }

    NeighborNodes findNeighborProcessors(const ValueTree &processor, ConnectionType connectionType, const ValueTree& excluding={}, bool before=true, bool after=true) {
        NeighborNodes neighborNodes {};
        const ValueTree &parent = processor.getParent();
        if (before) {
            getAllNodesFlowingInto(parent, processor, neighborNodes.before, connectionType, excluding);
        }
        if (after) {
            const ValueTree &afterNodeState = findProcessorToFlowInto(parent, processor, connectionType, excluding);
            neighborNodes.after = getNodeIdForState(afterNodeState);
        }

        return neighborNodes;
    }

    void getAllNodesFlowingInto(const ValueTree& parent, const ValueTree &processor, Array<NodeID>& nodes,
                                ConnectionType connectionType, const ValueTree& excluding={}) {
        if (!processor.hasType(IDs::PROCESSOR))
            return;

        int siblingDelta = 0;
        ValueTree otherParent;
        while ((otherParent = parent.getSibling(siblingDelta--)).isValid()) {
            for (const auto &otherProcessor : otherParent) {
                if (otherProcessor != excluding &&
                    findProcessorToFlowInto(otherParent, otherProcessor, connectionType) == processor) {
                    nodes.add(getNodeIdForState(otherProcessor));
                }
            }
        }
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
                bool isBelow = int(otherProcessor[IDs::processorSlot]) > int(processor[IDs::processorSlot]);
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

        const auto firstMasterTrackProcessorWithInputs = getFirstMasterTrackProcessorWithInputs(connectionType, excluding);
        if (firstMasterTrackProcessorWithInputs.isValid() && processor.getParent() != project.getMasterTrack())
            return firstMasterTrackProcessorWithInputs;

        return project.getAudioOutputProcessorState();
    }

    inline bool isProcessorAProducer(const ValueTree &processor, ConnectionType connectionType) const {
        return (connectionType == audio && int(processor[IDs::numOutputChannels]) > 0) ||
               (connectionType == midi && processor[IDs::producesMidi]);
    }

    inline bool isProcessorAnEffect(const ValueTree &processor, ConnectionType connectionType) const {
        return (connectionType == audio && int(processor[IDs::numInputChannels]) > 0) ||
               (connectionType == midi && processor[IDs::acceptsMidi]);
    }

    inline bool canProcessorDefaultConnectTo(const ValueTree &parent, const ValueTree &processor,
                                                ValueTree &otherParent, const ValueTree &otherProcessor,
                                                ConnectionType connectionType) const {
        if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor)
            return false;

        bool canConnectAudio = int(processor[IDs::numOutputChannels]) > 0 && int(otherProcessor[IDs::numInputChannels]) > 0;
        bool canConnectMidi = processor[IDs::producesMidi] && otherProcessor[IDs::acceptsMidi];
        return (connectionType == audio && canConnectAudio) || (connectionType == midi && canConnectMidi);
    }

    const ValueTree getFirstMasterTrackProcessorWithInputs(ConnectionType connectionType, const ValueTree& excluding={}) const {
        for (const auto& processor : project.getMasterTrack()) {
            if (processor.hasType(IDs::PROCESSOR) && processor != excluding && isProcessorAnEffect(processor, connectionType)) {
                return processor;
            }
        }
        return {};
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& property) override {
        if (tree.hasType(IDs::PROCESSOR)) {
            if (property == IDs::processorSlot) {
            } else if (property == IDs::bypassed) {
                if (auto node = getNodeForState(tree)) {
                    node->setBypassed(tree[IDs::bypassed]);
                }
            } else if (property == IDs::numInputChannels || property == IDs::numOutputChannels) {
                removeIllegalConnections();
            }
        }
    }

    void recursivelyInitializeWithState(const ValueTree &state) {
        if (state.hasType(IDs::PROCESSOR)) {
            addProcessor(state);
            return;
        }
        for (const ValueTree& child : state) {
            recursivelyInitializeWithState(child);
        }
    }

    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (currentlyDraggingNodeId == NA_NODE_ID) {
                if (getProcessorWrapperForState(child) == nullptr) {
                    addProcessor(child);
                }
            }
            // Kind of crappy - the order of the listeners seems to be nondeterministic,
            // so send (maybe _another_) select message that will update the UI in case this was already selected.
            if (child[IDs::selected]) {
                child.sendPropertyChangeMessage(IDs::selected);
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
            }
        } else if (child.hasType(IDs::TRACK) || child.hasType(IDs::MASTER_TRACK)) {
            recursivelyInitializeWithState(child);
        }
    }

    void valueTreeChildRemoved(ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (currentlyDraggingNodeId == NA_NODE_ID && !isMoving) {
                if (auto *node = getNodeForState(child)) {
                    // disconnect should have already been called before delete! (to avoid nested undo actions)
                    if (auto* processorWrapper = getProcessorWrapperForNodeId(node->nodeID)) {
                        if (child[IDs::name] == MidiInputProcessor::name()) {
                            if (auto *midiInputProcessor = dynamic_cast<MidiInputProcessor *>(processorWrapper->processor)) {
                                const String &deviceName = child.getProperty(IDs::deviceName);
                                deviceManager.removeMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
                            }
                        }
                        processerWrappers.removeObject(processorWrapper);
                    }
                    nodes.removeObject(node);
                    topologyChanged();
                }
            }
            for (int i = activePluginWindows.size(); --i >= 0;) {
                if (!nodes.contains(activePluginWindows.getUnchecked(i)->node)) {
                    activePluginWindows.remove(i);
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

                        source->outputs.removeAllInstancesOf({dest, destChannel, sourceChannel});
                        dest->inputs.removeAllInstancesOf({source, sourceChannel, destChannel});
                        topologyChanged();
                    }
                }
            }
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
        addDefaultConnections(processor);
        resetDefaultExternalInputConnections();
    };

    void processorWillBeDestroyed(const ValueTree& processor) override {
        removeDefaultConnections(processor);
        disconnectNode(getNodeIdForState(processor));
    };

    void processorHasBeenDestroyed(const ValueTree& processor) override {
        resetDefaultExternalInputConnections();
    };

    void itemSelected(const ValueTree&) override {};

    void itemRemoved(const ValueTree&) override {};
};
