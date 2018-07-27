#pragma once

#include <Identifiers.h>
#include <view/PluginWindow.h>
#include <processors/StatefulAudioProcessorWrapper.h>

#include "JuceHeader.h"
#include "processors/ProcessorManager.h"
#include "Project.h"

class ProcessorGraph : public AudioProcessorGraph, private ValueTree::Listener, private ProjectChangeListener, private ChangeListener {
public:
    explicit ProcessorGraph(Project &project, UndoManager &undoManager)
            : project(project), undoManager(undoManager) {
        enableAllBuses();

        this->project.getTracks().addListener(this);
        this->project.getConnections().addListener(this);
        addProcessor(project.getAudioOutputProcessorState());
        recursivelyInitializeWithState(project.getMasterTrack());
        recursivelyInitializeWithState(project.getTracks());

        if (project.hasConnections()) {
            for (auto connection : project.getConnections()) {
                valueTreeChildAdded(project.getConnections(), connection);
            }
        } else {
            recursivelyInitializeWithState(project.getMasterTrack(), true);
            recursivelyInitializeWithState(project.getTracks(), true);
        }
        this->project.addChangeListener(this);
        this->addChangeListener(this);
        initializing = false;
    }

    bool isSelected(NodeID nodeId) {
        if (auto* processor = getProcessorWrapperForNodeId(nodeId)) {
            return processor->state[IDs::selected];
        }

        return false;
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return getProcessorWrapperForNodeId(getNodeIdForState(processorState));
    }

    static const NodeID getNodeIdForState(const ValueTree &processorState) {
        return processorState.isValid() ? NodeID(int(processorState[IDs::NODE_ID])) : NA_NODE_ID;
    }

    Node* getNodeForState(const ValueTree &processorState) const {
        return getNodeForId(getNodeIdForState(processorState));
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(NodeID nodeId) const {
        if (nodeId == NA_NODE_ID) {
            return nullptr;
        } else {
            if (auto* node = getNodeForId(nodeId)) {
                AudioProcessor *processor = node->getProcessor();
                for (auto *processorWrapper : processerWrappers) {
                    if (processorWrapper->processor == processor) {
                        return processorWrapper;
                    }
                }
            }

            return nullptr;
        }
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

            if (processorWrapper->processor->getName() == MixerChannelProcessor::getIdentifier())
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
        int fromSlot = processorState[IDs::PROCESSOR_SLOT];
        if (fromSlot == toSlot && processorState.getParent() == toTrack)
            return;

        processorWillBeMoved(processorState, toTrack);
        processorState.setProperty(IDs::PROCESSOR_SLOT, toSlot, getDragDependentUndoManager());
        const int insertIndex = project.getParentIndexForProcessor(toTrack, processorState, getDragDependentUndoManager());
        Helpers::moveSingleItem(processorState, toTrack, insertIndex, getDragDependentUndoManager());
        processorHasMoved(processorState, toTrack);
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
        return project.removeConnection(c, getDragDependentUndoManager(), false);
    }

    bool addDefaultConnection(const Connection& c) {
        if (canConnectUi(c)) {
            project.addConnection(c, getDragDependentUndoManager());
            return true;
        }
        return false;
    }

    bool removeDefaultConnection(const Connection& c) {
        return project.removeConnection(c, getDragDependentUndoManager(), true);
    }

    // only called when removing a node. we want to make sure we don't use the undo manager for these connection removals.
    bool disconnectNode(NodeID nodeId) override {
        return doDisconnectNode(nodeId, false);
    }

    bool removeNode(NodeID nodeId) override  {
        if (auto* processorWrapper = getProcessorWrapperForNodeId(nodeId)) {
            disconnectNode(nodeId);
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
    OwnedArray<StatefulAudioProcessorWrapper> processerWrappers;
    OwnedArray<PluginWindow> activePluginWindows;
    Array<NodeID> neighborsOfLastRemovedProcessor;

    bool initializing { true };
    bool isMoving { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    inline UndoManager* getDragDependentUndoManager() {
        return currentlyDraggingNodeId == NA_NODE_ID ? &undoManager : nullptr;
    }

    bool doDisconnectNode(NodeID nodeId, bool defaultOnly) {
        const Array<ValueTree> connections = project.getConnectionsForNode(nodeId);
        bool anyRemoved = false;
        for (const auto &connection : connections)
            if (project.removeConnection(connection, &undoManager, defaultOnly)) {
                anyRemoved = true;
                NodeID sourceNodeId = getNodeIdForState(connection.getChildWithName(IDs::SOURCE));
                NodeID destinationNodeId = getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
                neighborsOfLastRemovedProcessor.add(sourceNodeId == nodeId ? destinationNodeId : sourceNodeId);
            }

        return anyRemoved;
    }

    void recursivelyInitializeWithState(const ValueTree &state, bool connections=false) {
        if (state.hasType(IDs::PROCESSOR)) {
            if (connections) {
                return addDefaultConnections(state);
            } else {
                return addProcessor(state);
            }
        }
        for (const ValueTree& child : state) {
            if (!child.hasType(IDs::MASTER_TRACK)) {
                recursivelyInitializeWithState(child, connections);
            }
        }
    }

    void addProcessor(const ValueTree &processorState) {
        static String errorMessage = "Could not create processor";
        PluginDescription *desc = project.getTypeForIdentifier(processorState[IDs::id]);
        auto *processor = project.getFormatManager().createPluginInstance(*desc, getSampleRate(), getBlockSize(), errorMessage);
        auto *processorWrapper = new StatefulAudioProcessorWrapper(processor, processorState, undoManager);
        processerWrappers.add(processorWrapper);

        const Node::Ptr &newNode = processorState.hasProperty(IDs::NODE_ID) ?
                                   addNode(processor, getNodeIdForState(processorState)) :
                                   addNode(processor);
        processorWrapper->state.setProperty(IDs::NODE_ID, int(newNode->nodeID), nullptr);
        processorWrapper->state.sendPropertyChangeMessage(IDs::BYPASSED);

        if (!initializing) {
            const Array<ValueTree> &nodeConnections = project.getConnectionsForNode(newNode->nodeID);
            for (auto& connection : nodeConnections) {
                valueTreeChildAdded(project.getConnections(), connection);
            }
        }
    }

    struct NeighborNodes {
        Array<NodeID> before {};
        NodeID after = NA_NODE_ID;
    };

    void removeDefaultConnections(const ValueTree &processor) {
        doDisconnectNode(getNodeIdForState(processor), true);
    }

    void addDefaultConnections(const ValueTree &processor) {
        const NeighborNodes &neighbors = findNeighborProcessors(processor);
        auto nodeId = getNodeIdForState(processor);
        if (!neighbors.before.isEmpty()) {
            for (auto before : neighbors.before) {
                for (int channel = 0; channel < 2; ++channel) {
                    removeDefaultConnection({{before, channel}, {neighbors.after, channel}});
                    addDefaultConnection({{before, channel}, {nodeId, channel}});
                }
            }
        }
        for (int channel = 0; channel < 2; ++channel) {
            addDefaultConnection({{nodeId, channel}, {neighbors.after, channel}});
        }
    }

    NeighborNodes findNeighborProcessors(const ValueTree &processor) {
        NeighborNodes neighborNodes {};
        const ValueTree &parent = processor.getParent();
        getAllNodesFlowingInto(parent, processor, neighborNodes.before);
        const ValueTree &afterNodeState = getFirstProcessorBelowOrToRightAndBelow(parent, processor);
        neighborNodes.after = afterNodeState.isValid() ? getNodeIdForState(afterNodeState) : project.getAudioOutputNodeId();
        jassert(neighborNodes.after != NA_NODE_ID);

        return neighborNodes;
    }

    void getAllNodesFlowingInto(const ValueTree& parent, const ValueTree &processor, Array<NodeID>& nodes) {
        if (!processor.hasType(IDs::PROCESSOR) || int(processor[IDs::NUM_INPUT_CHANNELS]) <= 0)
            return;

        int slot = processor[IDs::PROCESSOR_SLOT];

        int siblingDelta = 0;
        ValueTree siblingParent;
        while ((siblingParent = parent.getSibling(siblingDelta--)).isValid()) {
            for (const auto &otherProcessor : siblingParent) {
                if (getFirstProcessorBelowOrToRightAndBelow(siblingParent, otherProcessor) == processor) {
                    nodes.add(getNodeIdForState(otherProcessor));
                }
            }
        }
    }

    const ValueTree getFirstProcessorBelowOrToRightAndBelow(const ValueTree &parent, const ValueTree &processor) {
        if (!processor.hasType(IDs::PROCESSOR) || int(processor[IDs::NUM_OUTPUT_CHANNELS]) <= 0)
            return {};

        int slot = processor[IDs::PROCESSOR_SLOT];

        int siblingDelta = 0;
        ValueTree siblingParent;
        while ((siblingParent = parent.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : siblingParent) {
                if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor || int(otherProcessor[IDs::NUM_INPUT_CHANNELS]) <= 0)
                    continue;
                int otherSlot = otherProcessor[IDs::PROCESSOR_SLOT];
                if (otherSlot > slot ||
                    (slot == Project::NUM_AVAILABLE_PROCESSOR_SLOTS - 1 && parent != project.getMasterTrack() && otherProcessor == project.getMasterTrack().getChildWithName(IDs::PROCESSOR)))
                    return otherProcessor;
            }
        }
        return {};
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& property) override {
        if (tree.hasType(IDs::PROCESSOR)) {
            if (property == IDs::PROCESSOR_SLOT) {
                sendChangeMessage(); // TODO get rid of?
            } else if (property == IDs::BYPASSED) {
                if (auto node = getNodeForState(tree)) {
                    node->setBypassed(tree[IDs::BYPASSED]);
                }
            }
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
                        int destChannel = destState[IDs::CHANNEL];
                        int sourceChannel = sourceState[IDs::CHANNEL];

                        source->outputs.add({dest, destChannel, sourceChannel});
                        dest->inputs.add({source, sourceChannel, destChannel});
                        topologyChanged();
                    }
                }
            }
            sendChangeMessage();
        }
    }

    void valueTreeChildRemoved(ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (currentlyDraggingNodeId == NA_NODE_ID && !isMoving) {
                if (auto *node = getNodeForState(child)) {
                    // disconnect should have already been called before delete! (to avoid nested undo actions)
                    if (auto* processorWrapper = getProcessorWrapperForNodeId(node->nodeID)) {
                        processerWrappers.removeObject(processorWrapper);
                    }
                    nodes.removeObject(node);
                    topologyChanged();
                }
            }
        } else if (child.hasType(IDs::CONNECTION)) {
            if (currentlyDraggingNodeId == NA_NODE_ID) {
                const ValueTree &sourceState = child.getChildWithName(IDs::SOURCE);
                const ValueTree &destState = child.getChildWithName(IDs::DESTINATION);

                if (auto *source = getNodeForState(sourceState)) {
                    if (auto *dest = getNodeForState(destState)) {
                        int destChannel = destState[IDs::CHANNEL];
                        int sourceChannel = sourceState[IDs::CHANNEL];

                        source->outputs.removeAllInstancesOf({dest, destChannel, sourceChannel});
                        dest->inputs.removeAllInstancesOf({source, sourceChannel, destChannel});
                        topologyChanged();
                    }
                }
            }
            sendChangeMessage();
        }
    }

    void valueTreeChildOrderChanged(ValueTree& parent, int oldIndex, int newIndex) override {
        if (parent.hasType(IDs::TRACKS) || parent.hasType(IDs::TRACK)) {
            sendChangeMessage();
        }
    }

    void valueTreeParentChanged(ValueTree& treeWhoseParentHasChanged) override {
    }

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
    };

    void processorWillBeDestroyed(const ValueTree& processor) override {
        jassert(neighborsOfLastRemovedProcessor.isEmpty());
        disconnectNode(getNodeIdForState(processor));
    };

    void processorHasBeenDestroyed(const ValueTree& processor) override {
        insertConnectionsForNodes(neighborsOfLastRemovedProcessor);
        neighborsOfLastRemovedProcessor.clear();
    };

    void processorWillBeMoved(const ValueTree& processor, const ValueTree& newParent) override {
        jassert(neighborsOfLastRemovedProcessor.isEmpty());
        removeDefaultConnections(processor);
    };

    void processorHasMoved(const ValueTree& processor, const ValueTree& newParent) override {
        addDefaultConnections(processor);
        project.makeSlotsValid(newParent, getDragDependentUndoManager());
        insertConnectionsForNodes(neighborsOfLastRemovedProcessor);
        neighborsOfLastRemovedProcessor.clear();
    };

    void insertConnectionsForNodes(Array<NodeID> nodeIds) {
        for (auto nodeId : nodeIds) {
            if (nodeId != NA_NODE_ID) {
                addDefaultConnections(getProcessorWrapperForNodeId(nodeId)->state);
            }
        }
    }
    
    void itemSelected(const ValueTree&) override {
        sendChangeMessage();
    };

    void itemRemoved(const ValueTree&) override {};

    void changeListenerCallback(ChangeBroadcaster* cb) override {
        if (cb == this) {
            for (int i = activePluginWindows.size(); --i >= 0;) {
                if (!getNodes().contains(activePluginWindows.getUnchecked(i)->node)) {
                    activePluginWindows.remove(i);
                }
            }
        }
    }
};
