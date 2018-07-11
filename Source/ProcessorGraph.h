#pragma once

#include <Identifiers.h>

#include "JuceHeader.h"
#include "ValueTreeItems.h"
#include "processors/ProcessorIds.h"

class ProcessorGraph : public AudioProcessorGraph, private ValueTree::Listener, private ProjectChangeListener {
public:
    explicit ProcessorGraph(Project &project, UndoManager &undoManager)
            : project(project), undoManager(undoManager) {
        enableAllBuses();
        audioOutputNode = addNode(new AudioGraphIOProcessor(AudioGraphIOProcessor::audioOutputNode));

        this->project.getConnections().addListener(this);
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
        this->project.getTracks().addListener(this);
        this->project.addChangeListener(this);
        initializing = false;
    }

    std::vector<Connection> getConnectionsUi() const {
        std::vector<Connection> graphConnections;

        for (auto connectionState : project.getConnections()) {
            const ValueTree &sourceState = connectionState.getChildWithName(IDs::SOURCE);
            const ValueTree &destinationState = connectionState.getChildWithName(IDs::DESTINATION);

            // nodes have already been added. just need to add connections if there are any.
            NodeAndChannel source{};
            source.nodeID = NodeID(int(sourceState[IDs::NODE_ID]));
            source.channelIndex = sourceState[IDs::CHANNEL];

            NodeAndChannel destination{};
            destination.nodeID = NodeID(int(destinationState[IDs::NODE_ID]));
            destination.channelIndex = destinationState[IDs::CHANNEL];

            graphConnections.emplace_back(source, destination);
        }

        return graphConnections;
    }

    bool isSelected(NodeID nodeId) {
        if (auto* processor = getProcessorForNodeId(nodeId)) {
            return processor->state[IDs::selected];
        }

        return false;
    }

    StatefulAudioProcessor *getProcessorForState(const ValueTree &processorState) const {
        return getProcessorForNodeId(getNodeIdForState(processorState));
    }

    const NodeID getNodeIdForState(const ValueTree &state) const {
        return NodeID(int(state[IDs::NODE_ID]));
    }

    StatefulAudioProcessor *getProcessorForNodeId(NodeID nodeId) const {
        if (nodeId == NA_NODE_ID) {
            return nullptr;
        } else {
            Node *node = getNodeForId(nodeId);
            return node != nullptr ? dynamic_cast<StatefulAudioProcessor *>(node->getProcessor()) : nullptr;
        }
    }

    StatefulAudioProcessor *getMasterGainProcessor() {
        const ValueTree masterTrack = project.getMasterTrack();
        const ValueTree &gain = masterTrack.getChildWithProperty(IDs::name, MixerChannelProcessor::name());
        return getProcessorForState(gain);
    }

    void beginDraggingNode(NodeID nodeId) {
        if (auto* processor = getProcessorForNodeId(nodeId)) {
            project.setSelectedProcessor(processor->state);

            if (processor->getName() == MixerChannelProcessor::name())
                // mixer channel processors are special processors.
                // they could be dragged and reconnected like any old processor, but please don't :)
                return;
            const Point<int> &gridLocation = getProcessorGridLocation(nodeId);
            currentlyDraggingNodeId = nodeId;
            currentlyDraggingGridPosition.setXY(gridLocation.x, gridLocation.y);
            initialDraggingGridPosition = currentlyDraggingGridPosition;
            project.makeConnectionsSnapshot();
        }
    }

    void setNodePosition(NodeID nodeId, Point<double> pos) {
        if (currentlyDraggingNodeId == NA_NODE_ID)
            return;

        if (auto *processor = getProcessorForNodeId(nodeId)) {
            int maxProcessorSlot = project.getMaxAvailableProcessorSlot(processor->state.getParent());
            auto newX = jlimit(0, project.getNumTracks() - 1, int(Project::NUM_VISIBLE_TRACKS * jlimit(0.0, 0.99, pos.x)));
            auto newY = jlimit(0, maxProcessorSlot, int(Project::NUM_VISIBLE_PROCESSOR_SLOTS * jlimit(0.0, 0.99, pos.y)));
            if (newX != currentlyDraggingGridPosition.x || newY != currentlyDraggingGridPosition.y) {
                currentlyDraggingGridPosition.x = newX;
                currentlyDraggingGridPosition.y = newY;

                moveProcessor(processor->state, initialDraggingGridPosition.x, initialDraggingGridPosition.y);
                project.restoreConnectionsSnapshot();
                if (currentlyDraggingGridPosition != initialDraggingGridPosition) {
                    moveProcessor(processor->state, currentlyDraggingGridPosition.x, currentlyDraggingGridPosition.y);
                }
            }
        }
    }

    void endDraggingNode(NodeID nodeId) {
        if (currentlyDraggingNodeId != NA_NODE_ID && currentlyDraggingGridPosition != initialDraggingGridPosition) {
            // update the audio graph to match the current preview UI graph.
            StatefulAudioProcessor *processor = getProcessorForNodeId(nodeId);
            moveProcessor(processor->state, initialDraggingGridPosition.x, initialDraggingGridPosition.y);
            project.restoreConnectionsSnapshot();
            currentlyDraggingNodeId = NA_NODE_ID;
            moveProcessor(processor->state, currentlyDraggingGridPosition.x, currentlyDraggingGridPosition.y);
        }
        currentlyDraggingNodeId = NA_NODE_ID;
    }

    void moveProcessor(ValueTree &processorState, int toTrackIndex, int toSlot) {
        const ValueTree &toTrack = project.getTrack(toTrackIndex);
        if (int(processorState[IDs::PROCESSOR_SLOT]) == toSlot && processorState.getParent() == toTrack)
            return;

        processorState.setProperty(IDs::PROCESSOR_SLOT, toSlot, getDragDependentUndoManager());

        const int insertIndex = project.getParentIndexForProcessor(toTrack, processorState, getDragDependentUndoManager());
        processorWillBeMoved(processorState, toTrack, insertIndex);
        Helpers::moveSingleItem(processorState, toTrack, insertIndex, getDragDependentUndoManager());
        processorHasMoved(processorState, toTrack);
    }

    Point<double> getNodePosition(NodeID nodeId) const {
        const Point<int> gridLocation = getProcessorGridLocation(nodeId);
        return {gridLocation.x / float(Project::NUM_VISIBLE_TRACKS) + (0.5 / Project::NUM_VISIBLE_TRACKS),
                gridLocation.y / float(Project::NUM_VISIBLE_PROCESSOR_SLOTS) + (0.5 / Project::NUM_VISIBLE_PROCESSOR_SLOTS)};
    }

    bool canConnectUi(const Connection& c) const {
        if (auto* source = getNodeForId (c.source.nodeID))
            if (auto* dest = getNodeForId (c.destination.nodeID))
                return canConnectUi(source, c.source.channelIndex,
                                   dest, c.destination.channelIndex);

        return false;
    }

    bool isConnectedUi(const Connection& c) const noexcept {
        return project.getConnectionMatching(c).isValid();
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
            || (!sourceIsMIDI && sourceChannel >= source->processor->getTotalNumOutputChannels())
            || (sourceIsMIDI && ! source->processor->producesMidi()))
            return false;

        if (dest == nullptr
            || (!destIsMIDI && destChannel >= dest->processor->getTotalNumInputChannels())
            || (destIsMIDI && ! dest->processor->acceptsMidi()))
            return false;

        return !isConnectedUi({{source->nodeID, sourceChannel}, {dest->nodeID, destChannel}});
    }

    bool addConnection(const Connection& c) override {
        if (canConnectUi(c)) {
            project.addConnection(c, getDragDependentUndoManager());
            return true;
        }
        return false;
    }

    bool removeConnection(const Connection& c) override {
        return project.removeConnection(c, getDragDependentUndoManager());
    }

    // only called when removing a node. we want to make sure we don't use the undo manager for these connection removals.
    bool disconnectNode(NodeID nodeId) override {
        const auto& processor = getProcessorForNodeId(nodeId)->state;
        removeNodeConnections(nodeId, processor.getParent(), findNeighborNodes(processor));

        const Array<ValueTree> remainingConnections = project.getConnectionsForNode(getNodeIdForState(processor));
        if (!remainingConnections.isEmpty()) {
            for (const auto &c : remainingConnections)
                project.removeConnection(c, &undoManager);
        }
        return false;
    }

    bool removeNode(NodeID nodeId) override  {
        if (auto* processor = getProcessorForNodeId(nodeId)) {
            disconnectNode(nodeId);
            processor->state.getParent().removeChild(processor->state, &undoManager);
            return true;
        }
        return false;
    }

private:
    const static NodeID NA_NODE_ID = 0;

    NodeID currentlyDraggingNodeId = NA_NODE_ID;
    Point<int> currentlyDraggingGridPosition;
    Point<int> initialDraggingGridPosition;

    Project &project;
    UndoManager &undoManager;
    Node::Ptr audioOutputNode;

    bool initializing { true };
    bool isMoving { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    inline UndoManager* getDragDependentUndoManager() {
        return currentlyDraggingNodeId == NA_NODE_ID ? &undoManager : nullptr;
    }

    void recursivelyInitializeWithState(const ValueTree &state, bool connections=false) {
        if (state.hasType(IDs::PROCESSOR)) {
            if (connections) {
                return insertNodeConnections(getNodeIdForState(state), state);
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
        auto *processor = createStatefulAudioProcessorFromId(processorState[IDs::name], processorState, undoManager);
        processor->updateValueTree();

        const Node::Ptr &newNode = processorState.hasProperty(IDs::NODE_ID) ?
                                   addNode(processor, getNodeIdForState(processorState)) :
                                   addNode(processor);
        processor->state.setProperty(IDs::NODE_ID, int(newNode->nodeID), nullptr);

        if (!initializing) {
            const Array<ValueTree> &nodeConnections = project.getConnectionsForNode(newNode->nodeID);
            for (auto& connection : nodeConnections) {
                valueTreeChildAdded(project.getConnections(), connection);
            }
        }
    }

    Point<int> getProcessorGridLocation(NodeID nodeId) const {
        if (nodeId == audioOutputNode->nodeID) {
            return {Project::NUM_VISIBLE_TRACKS - 1, Project::NUM_VISIBLE_PROCESSOR_SLOTS - 1};
        } else if (auto *processor = getProcessorForNodeId(nodeId)) {
            auto column = processor->state.getParent().getParent().indexOf(processor->state.getParent());
            auto row = int(processor->state[IDs::PROCESSOR_SLOT]);
            return {column, row};
        } else {
            return {0, 0};
        }
    }

    struct NeighborNodes {
        NodeID before = NA_NODE_ID, after = NA_NODE_ID;
    };

    void removeNodeConnections(NodeID nodeId, const ValueTree& parent, NeighborNodes neighborNodes) {
        for (int channel = 0; channel < 2; ++channel) {
            removeConnection({{nodeId, channel},
                              {neighborNodes.after, channel}});
        }

        if (neighborNodes.before != NA_NODE_ID) {
            for (int channel = 0; channel < 2; ++channel) {
                removeConnection({{neighborNodes.before, channel},
                                  {nodeId, channel}});
                addConnection({{neighborNodes.before, channel},
                               {neighborNodes.after, channel}});
            }
        } else if (parent.hasType(IDs::MASTER_TRACK)) {
            // first processor in master track receives connections from the last processor of every track
            for (int i = 0; i < project.getNumTracks(); i++) {
                const ValueTree lastProcessor = getLastProcessorInTrack(project.getTrack(i));
                if (lastProcessor.isValid()) {
                    NodeID lastProcessorNodeId = getNodeIdForState(lastProcessor);
                    for (int channel = 0; channel < 2; ++channel) {
                        removeConnection({{lastProcessorNodeId, channel},
                                          {nodeId, channel}});
                        addConnection({{lastProcessorNodeId, channel},
                                       {neighborNodes.after, channel}});
                    }
                }
            }
        }
    }

    void insertNodeConnections(NodeID nodeId, const ValueTree& nodeState) {
        NeighborNodes neighborNodes = findNeighborNodes(nodeState);

        if (neighborNodes.before != NA_NODE_ID) {
            for (int channel = 0; channel < 2; ++channel) {
                removeConnection({{neighborNodes.before, channel},
                                  {neighborNodes.after,  channel}});
                addConnection({{neighborNodes.before, channel},
                               {nodeId,  channel}});
            }
        } else if (nodeState.getParent().hasType(IDs::MASTER_TRACK)) {
            // first processor in master track receives connections from the last processor of every track
            for (int i = 0; i < project.getNumTracks(); i++) {
                const ValueTree lastProcessor = getLastProcessorInTrack(project.getTrack(i));
                if (lastProcessor.isValid()) {
                    NodeID lastProcessorNodeId = getNodeIdForState(lastProcessor);
                    for (int channel = 0; channel < 2; ++channel) {
                        removeConnection({{lastProcessorNodeId, channel},
                                          {neighborNodes.after,  channel}});
                        addConnection({{lastProcessorNodeId, channel},
                                       {nodeId,  channel}});
                    }
                }
            }
        }
        for (int channel = 0; channel < 2; ++channel) {
            addConnection({{nodeId, channel},
                           {neighborNodes.after,  channel}});
        }
    }

    NeighborNodes findNeighborNodes(const ValueTree& nodeState) {
        const ValueTree &before = nodeState.getSibling(-1);
        const ValueTree &after = nodeState.getSibling(1);

        return findNeighborNodes(nodeState.getParent(), before, after);
    }

    NeighborNodes findNeighborNodes(const ValueTree& parent, const ValueTree& beforeNodeState, const ValueTree& afterNodeState) {
        NeighborNodes neighborNodes;

        if (beforeNodeState.isValid()) {
            neighborNodes.before = getNodeIdForState(beforeNodeState);
        }
        if (!afterNodeState.isValid() || (neighborNodes.after = getNodeIdForState(afterNodeState)) == NA_NODE_ID) {
            if (parent.hasType(IDs::MASTER_TRACK)) {
                neighborNodes.after = audioOutputNode->nodeID;
            } else {
                neighborNodes.after = getNodeIdForState(project.getMasterTrack().getChildWithName(IDs::PROCESSOR));
                if (neighborNodes.after == NA_NODE_ID) { // master track has no processors. go straight out.
                    neighborNodes.after = audioOutputNode->nodeID;
                }
            }
        }
        jassert(neighborNodes.after != NA_NODE_ID);

        return neighborNodes;
    }

    const ValueTree getLastProcessorInTrack(const ValueTree &track) {
        if (!track.hasType(IDs::TRACK))
            return ValueTree();
        for (int j = track.getNumChildren() - 1; j >= 0; j--) {
            if (track.getChild(j).hasType(IDs::PROCESSOR))
                return track.getChild(j);
        }
        return ValueTree();
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& property) override {
        if (property == IDs::PROCESSOR_SLOT) {
            sendChangeMessage();
        }
    }

    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (currentlyDraggingNodeId == NA_NODE_ID) {
                if (getProcessorForState(child) == nullptr) {
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

                Node *source = getNodeForId(getNodeIdForState(sourceState));
                Node *dest = getNodeForId(getNodeIdForState(destState));
                int destChannel = destState[IDs::CHANNEL];
                int sourceChannel = sourceState[IDs::CHANNEL];

                source->outputs.add({dest, destChannel, sourceChannel});
                dest->inputs.add({source, sourceChannel, destChannel});
                topologyChanged();
            }
            sendChangeMessage();
        }
    }

    void valueTreeChildRemoved(ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (currentlyDraggingNodeId == NA_NODE_ID && !isMoving) {
                if (auto nodeId = getNodeIdForState(child)) {
                    if (auto *node = getNodeForId(nodeId)) {
                        // disconnect should have already been called before delete! (to avoid nested undo actions)
                        nodes.removeObject(node);
                        topologyChanged();
                    }
                }
            }
        } else if (child.hasType(IDs::CONNECTION)) {
            if (currentlyDraggingNodeId == NA_NODE_ID) {
                const ValueTree &sourceState = child.getChildWithName(IDs::SOURCE);
                const ValueTree &destState = child.getChildWithName(IDs::DESTINATION);

                Node *source = getNodeForId(getNodeIdForState(sourceState));
                Node *dest = getNodeForId(getNodeIdForState(destState));
                int destChannel = destState[IDs::CHANNEL];
                int sourceChannel = sourceState[IDs::CHANNEL];

                source->outputs.removeAllInstancesOf({dest, destChannel, sourceChannel});
                dest->inputs.removeAllInstancesOf({source, sourceChannel, destChannel});
                topologyChanged();

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
        insertNodeConnections(getNodeIdForState(processor), processor);
    };

    void processorWillBeDestroyed(const ValueTree& processor) override {
        disconnectNode(getNodeIdForState(processor));
    };

    void processorWillBeMoved(const ValueTree& processor, const ValueTree& newParent, int insertIndex) override {
        isMoving = true;
        NodeID nodeId = getNodeIdForState(processor);
        removeNodeConnections(nodeId, processor.getParent(), findNeighborNodes(processor));
    };

    void processorHasMoved(const ValueTree& processor, const ValueTree& newParent) override {
        NodeID nodeId = getNodeIdForState(processor);
        insertNodeConnections(nodeId, processor);
        project.makeSlotsValid(newParent, getDragDependentUndoManager());
        isMoving = false;
    };

    void itemSelected(const ValueTree&) override {
        sendChangeMessage();
    };

    void itemRemoved(const ValueTree&) override {};
};
