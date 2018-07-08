#pragma once

#include <Identifiers.h>

#include "JuceHeader.h"
#include "ValueTreeItems.h"
#include "processors/ProcessorIds.h"

class ProcessorGraph : public AudioProcessorGraph, private ValueTree::Listener {
public:
    explicit ProcessorGraph(Project &project, UndoManager &undoManager)
            : project(project), undoManager(undoManager) {
        enableAllBuses();
        audioOutputNode = addNode(new AudioGraphIOProcessor(AudioGraphIOProcessor::audioOutputNode));

        bool hasConnections = project.hasConnections();

        recursivelyInitializeWithState(project.getMasterTrack(), !hasConnections);
        recursivelyInitializeWithState(project.getTracks(), !hasConnections);

        if (hasConnections) {
            for (auto connection : project.getConnections()) {
                addConnection(connection);
            }
        } else {
            topologyChanged(); // write out any new connections
        }
        initializing = false;
        this->project.getTracks().addListener(this);
    }

    StatefulAudioProcessor *getProcessorForState(const ValueTree &processorState) const {
        return getProcessorForNodeId(getNodeIdForProcessorState(processorState));
    }

    const NodeID getNodeIdForProcessorState(const ValueTree &processorState) const {
        return NodeID(int(processorState[IDs::NODE_ID]));
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
        const Point<int> &gridLocation = getProcessorGridLocation(nodeId);
        currentlyDraggingGridPosition.setXY(gridLocation.x, gridLocation.y);
        currentlyDraggingNodeId = nodeId;
        initialDraggingGridPosition = currentlyDraggingGridPosition;
    }

    void setNodePosition(NodeID nodeId, Point<double> pos) {
        currentlyDraggingGridPosition.x = int(Project::NUM_VISIBLE_TRACK_SLOTS * jlimit(0.0, 0.99, pos.x));
        currentlyDraggingGridPosition.y = int(Project::NUM_VISIBLE_TRACK_SLOTS * jlimit(0.0, 0.99, pos.y));
    }

    void endDraggingNode(NodeID nodeId) {
        if (currentlyDraggingNodeId) {
            // finalize positions
            for (Node *node : getNodes()) {
                if (node->nodeID == audioOutputNode->nodeID)
                    continue;
                // XXX wastefully getting processor from node id twice here.
                StatefulAudioProcessor *processor = getProcessorForNodeId(node->nodeID);
                processor->state.setProperty(IDs::PROCESSOR_SLOT, getProcessorGridLocation(node->nodeID).y, &undoManager);
            }
            StatefulAudioProcessor *processor = getProcessorForNodeId(nodeId);
            const ValueTree &track = project.getTrack(currentlyDraggingGridPosition.x);
            Helpers::moveSingleItem(processor->state, track, project.getParentIndexForProcessor(track, processor->state), undoManager);
        }
        currentlyDraggingNodeId = NA_NODE_ID;
    }
    
    Point<double> getNodePosition(NodeID nodeId) const {
        int row = 0, column = 0;

        if (nodeId == audioOutputNode->nodeID) {
            row = 7; column = 7;
        } else {
            const Point<int> gridLocation = getProcessorGridLocation(nodeId);
            row = gridLocation.y;
            column = gridLocation.x;
        }

        return {column / float(Project::NUM_VISIBLE_TRACK_SLOTS) + (0.5 / Project::NUM_VISIBLE_TRACK_SLOTS),
                row / float(Project::NUM_VISIBLE_TRACK_SLOTS) + (0.5 / Project::NUM_VISIBLE_TRACK_SLOTS)};
    }

    std::vector<Connection> getConnections() const {
        return project.getConnections();
    }

    Node::Ptr addNode(AudioProcessor* newProcessor, NodeID nodeId = {}) {
        const Node::Ptr &node = AudioProcessorGraph::addNode(newProcessor, nodeId);
        topologyChanged();
        return node;
    }

    bool removeNode(NodeID nodeId) {
        if (AudioProcessorGraph::removeNode(nodeId)) {
            topologyChanged();
            return true;
        }
        return false;
    }
    
    bool addConnection(const Connection& connection) {
        if (AudioProcessorGraph::addConnection(connection)) {
            topologyChanged();
            return true;
        }
        return false;
    }
    
    bool removeConnection(const Connection& connection) {
        if (AudioProcessorGraph::removeConnection(connection)) {
            topologyChanged();
            return true;
        }
        return false;
    }

    void clear() {
        AudioProcessorGraph::clear();
        topologyChanged();
    }

private:
    const static NodeID NA_NODE_ID = 0;
    bool initializing { true };
    
    NodeID currentlyDraggingNodeId = NA_NODE_ID;
    Point<int> currentlyDraggingGridPosition;
    Point<int> initialDraggingGridPosition;

    Project &project;
    UndoManager &undoManager;
    Node::Ptr audioOutputNode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    // called when graph topology changes
    void topologyChanged() {
        if (!initializing) {
            project.setConnections(AudioProcessorGraph::getConnections());
        }
    }

    void recursivelyInitializeWithState(const ValueTree &state, bool addDefaultConnections=true) {
        if (state.hasType(IDs::PROCESSOR)) {
            return addProcessor(state, addDefaultConnections);
        }
        for (int i = 0; i < state.getNumChildren(); i++) {
            const ValueTree &child = state.getChild(i);
            if (!child.hasType(IDs::MASTER_TRACK)) {
                recursivelyInitializeWithState(child, addDefaultConnections);
            }
        }
    }

    void addProcessor(const ValueTree &processorState, bool addDefaultConnections=true) {
        auto *processor = createStatefulAudioProcessorFromId(processorState[IDs::name], processorState, undoManager);
        processor->updateValueTree();
        
        const Node::Ptr &newNode = processorState.hasProperty(IDs::NODE_ID) ?
                                   addNode(processor, getNodeIdForProcessorState(processorState)) :
                                   addNode(processor);
        processor->state.setProperty(IDs::NODE_ID, int(newNode->nodeID), nullptr);
        if (addDefaultConnections) {
            insertNodeConnections(newNode->nodeID, processorState);
        }
    }

    Point<int> getProcessorGridLocation(NodeID nodeId) const {
        if (nodeId == currentlyDraggingNodeId) {
            return currentlyDraggingGridPosition;
        } else if (auto *processor = getProcessorForNodeId(nodeId)) {
            auto row = int(processor->state[IDs::PROCESSOR_SLOT]);
            auto column = processor->state.getParent().getParent().indexOf(processor->state.getParent());

            if (currentlyDraggingNodeId != NA_NODE_ID &&
                currentlyDraggingGridPosition.x == column) {
                if (initialDraggingGridPosition.y < row && row <= currentlyDraggingGridPosition.y) {
                    // move row _up_ towards first available slot
                    return {column, row - 1};
                } else if (currentlyDraggingGridPosition.y <= row && row < initialDraggingGridPosition.y) {
                    // move row _down_ towards first available slot
                    return {column, row + 1};
                } else {
                    return {column, row};
                }
            } else {
                return {column, row};
            }
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
                              {neighborNodes.after,  channel}});
        }

        if (neighborNodes.before != NA_NODE_ID) {
            for (int channel = 0; channel < 2; ++channel) {
                removeConnection({{neighborNodes.before, channel},
                                  {nodeId,  channel}});
                addConnection({{neighborNodes.before, channel},
                               {neighborNodes.after,  channel}});
            }
        } else if (parent.hasType(IDs::MASTER_TRACK)) {
            // first processor in master track receives connections from the last processor of every track
            for (int i = 0; i < project.getNumTracks(); i++) {
                const ValueTree lastProcessor = getLastProcessorInTrack(project.getTrack(i));
                if (lastProcessor.isValid()) {
                    NodeID lastProcessorNodeId = getNodeIdForProcessorState(lastProcessor);
                    for (int channel = 0; channel < 2; ++channel) {
                        removeConnection({{lastProcessorNodeId, channel},
                                          {nodeId,  channel}});
                        addConnection({{lastProcessorNodeId, channel},
                                       {neighborNodes.after,  channel}});
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
                    NodeID lastProcessorNodeId = getNodeIdForProcessorState(lastProcessor);
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
            neighborNodes.before = getNodeIdForProcessorState(beforeNodeState);
        }
        if (!afterNodeState.isValid() || (neighborNodes.after = getNodeIdForProcessorState(afterNodeState)) == NA_NODE_ID) {
            if (parent.hasType(IDs::MASTER_TRACK)) {
                neighborNodes.after = audioOutputNode->nodeID;
            } else {
                neighborNodes.after = getNodeIdForProcessorState(project.getMasterTrack().getChildWithName(IDs::PROCESSOR));
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

    void valueTreePropertyChanged (ValueTree& tree, const Identifier& property) override {}

    void valueTreeChildAdded (ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            addProcessor(child);
            project.makeSlotsValid(parent);

            // Kind of crappy - the order of the listeners seems to be nondeterministic,
            // so send (maybe _another_) select message that will update the UI in case this was already selected.
            if (child[IDs::selected]) {
                child.sendPropertyChangeMessage(IDs::selected);
            }
        }
    }

    void valueTreeChildRemoved (ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            NodeID removedNodeId = getNodeIdForProcessorState(child);

            removeNodeConnections(removedNodeId, parent, findNeighborNodes(parent, parent.getChild(indexFromWhichChildWasRemoved - 1), parent.getChild(indexFromWhichChildWasRemoved)));
            removeNode(removedNodeId);
        }
    }

    void valueTreeChildOrderChanged (ValueTree& parent, int oldIndex, int newIndex) override {
        ValueTree nodeState = parent.getChild(newIndex);
        if (nodeState.hasType(IDs::PROCESSOR)) {
            NodeID nodeId = getNodeIdForProcessorState(nodeState);
            const NeighborNodes &neighborNodes = oldIndex < newIndex ?
                                                 findNeighborNodes(parent, parent.getChild(oldIndex - 1), parent.getChild(oldIndex)) :
                                                 findNeighborNodes(parent, parent.getChild(oldIndex), parent.getChild(oldIndex + 1));
            removeNodeConnections(nodeId, parent, neighborNodes);
            insertNodeConnections(nodeId, nodeState);
            project.makeSlotsValid(parent);
        }
    }

    void valueTreeParentChanged (ValueTree& treeWhoseParentHasChanged) override {
    }

    void valueTreeRedirected (ValueTree& treeWhichHasBeenChanged) override {}
};
