#pragma once

#include <Identifiers.h>

#include "JuceHeader.h"
#include "ValueTreeItems.h"
#include <processors/SineBank.h>
#include <processors/GainProcessor.h>
#include <processors/BalanceAndGainProcessor.h>

static StatefulAudioProcessor *createStatefulAudioProcessorFromId(const String &id, const ValueTree &state, UndoManager &undoManager) {
    if (id == SineBank::name()) {
        return new SineBank(state, undoManager);
    } else if (id == GainProcessor::name()) {
        return new GainProcessor(state, undoManager);
    } else if (id == BalanceAndGainProcessor::name()) {
        return new BalanceAndGainProcessor(state, undoManager);
    } else {
        return nullptr;
    }
}

class ProcessorGraph : public AudioProcessorGraph, private ValueTree::Listener {
public:
    explicit ProcessorGraph(const ValueTree &projectState, UndoManager &undoManager)
            : projectState(projectState), undoManager(undoManager) {
        this->projectState.addListener(this);
        enableAllBuses();
        audioOutputNode = addNode(new AudioGraphIOProcessor(AudioGraphIOProcessor::audioOutputNode));

        recursivelyInitializeWithState(getMasterTrack());
        recursivelyInitializeWithState(projectState);
    }

    StatefulAudioProcessor *getProcessorForNodeId(NodeID nodeId) const {
        return nodeId != NA_NODE_ID ? dynamic_cast<StatefulAudioProcessor *>(getNodeForId(nodeId)->getProcessor()) : nullptr;
    }

    StatefulAudioProcessor *getProcessorForUuid(String &uuid) const {
        return getProcessorForNodeId(getNodeID(uuid));
    }

    const ValueTree getMasterTrack() {
        return projectState.getChildWithName(IDs::MASTER_TRACK);
    }

    StatefulAudioProcessor *getMasterGainProcessor() {
        const ValueTree masterTrack = getMasterTrack();
        ValueTree gain = masterTrack.getChildWithProperty(IDs::name, BalanceAndGainProcessor::name());
        String uuid = gain[IDs::uuid];
        return getProcessorForUuid(uuid);
    }

    const NodeID getNodeID(String &uuid) const {
        auto pair = nodeIdForUuid.find(uuid);
        return pair != nodeIdForUuid.end() ? pair->second : NA_NODE_ID;
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
            Helpers::moveSingleItem(processor->state, projectState.getChild(currentlyDraggingGridPosition.x), getParentIndexForProcessor(processor->state), undoManager);
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

    Point<int> getProcessorGridLocation(NodeID nodeId) const {
        if (nodeId == currentlyDraggingNodeId) {
            return currentlyDraggingGridPosition;
        } else if (auto *processor = getProcessorForNodeId(nodeId)) {
            auto row = int(processor->state.getProperty(IDs::PROCESSOR_SLOT));
            auto column = processor->state.getParent().getParent().indexOf(processor->state.getParent());

            if (currentlyDraggingNodeId != NA_NODE_ID &&
                currentlyDraggingGridPosition.x == column &&
                currentlyDraggingGridPosition.y != initialDraggingGridPosition.y) {
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
    
    // TODO consider moving this (and a bunch of other similar logic) to ValueTreeItems::Track
    // and using a 'Project' here instead of 'ValueTree' project state 
    int getParentIndexForProcessor(const ValueTree &processorState) {
        const ValueTree &track = processorState.getParent();
        if (track.getNumChildren() == 0) {
            return 0;
        }
        for (int i = 0; track.getNumChildren(); i++) {
            const ValueTree &otherProcessorState = track.getChild(i);
            if (otherProcessorState.hasType(IDs::PROCESSOR) &&
                int(otherProcessorState.getProperty(IDs::PROCESSOR_SLOT)) >= int(processorState.getProperty(IDs::PROCESSOR_SLOT))) {
                return track.indexOf(otherProcessorState);
            }
        }

        // TODO in this and other places, we assume processers are the only type of track child.
        return track.getNumChildren() - 1;
    }
    
private:
    const static NodeID NA_NODE_ID = 0;

    NodeID currentlyDraggingNodeId = NA_NODE_ID;
    Point<int> currentlyDraggingGridPosition;
    Point<int> initialDraggingGridPosition;
    ValueTree projectState;
    UndoManager &undoManager;
    Node::Ptr audioOutputNode;
    std::unordered_map<String, NodeID> nodeIdForUuid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    const NodeID getNodeIdFromProcessorState(const ValueTree &processorState) {
        String uuid = processorState[IDs::uuid].toString();
        return getNodeID(uuid);
    }

    void recursivelyInitializeWithState(const ValueTree &state) {
        if (state.hasType(IDs::PROCESSOR)) {
            return addProcessor(state);
        }
        for (int i = 0; i < state.getNumChildren(); i++) {
            const ValueTree &child = state.getChild(i);
            if (!child.hasType(IDs::MASTER_TRACK)) {
                recursivelyInitializeWithState(child);
            }
        }
    }

    void addProcessor(const ValueTree &processorState) {
        auto *processor = createStatefulAudioProcessorFromId(processorState[IDs::name], processorState, undoManager);
        processor->updateValueTree();

        const Node::Ptr &newNode = addNode(processor);
        nodeIdForUuid.insert(std::pair<String, NodeID>(processor->state[IDs::uuid].toString(), newNode->nodeID));
        insertNodeConnections(newNode->nodeID, processorState);
        // Kind of crappy - the order of the listeners seems to be nondeterministic,
        // so send (maybe _another_) select message that will update the UI in case this was already selected.
        if (processorState[IDs::selected]) {
            processor->state.sendPropertyChangeMessage(IDs::selected);
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
            for (int i = 0; i < projectState.getNumChildren(); i++) {
                const ValueTree lastProcessor = getLastProcessorInTrack(projectState.getChild(i));
                if (lastProcessor.isValid()) {
                    NodeID lastProcessorNodeId = getNodeIdFromProcessorState(lastProcessor);
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
            for (int i = 0; i < projectState.getNumChildren(); i++) {
                const ValueTree lastProcessor = getLastProcessorInTrack(projectState.getChild(i));
                if (lastProcessor.isValid()) {
                    NodeID lastProcessorNodeId = getNodeIdFromProcessorState(lastProcessor);
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
            neighborNodes.before = getNodeIdFromProcessorState(beforeNodeState);
        }
        if (!afterNodeState.isValid() || (neighborNodes.after = getNodeIdFromProcessorState(afterNodeState)) == NA_NODE_ID) {
            if (parent.hasType(IDs::MASTER_TRACK)) {
                neighborNodes.after = audioOutputNode->nodeID;
            } else {
                neighborNodes.after = getNodeIdFromProcessorState(projectState.getChildWithName(IDs::MASTER_TRACK).getChildWithName(IDs::PROCESSOR));
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
        }
    }

    void valueTreeChildRemoved (ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            NodeID removedNodeId = getNodeIdFromProcessorState(child);

            removeNodeConnections(removedNodeId, parent, findNeighborNodes(parent, parent.getChild(indexFromWhichChildWasRemoved - 1), parent.getChild(indexFromWhichChildWasRemoved)));

            nodeIdForUuid.erase(child[IDs::uuid]);
            removeNode(removedNodeId);
        }
    }

    void valueTreeChildOrderChanged (ValueTree& parent, int oldIndex, int newIndex) override {
        ValueTree nodeState = parent.getChild(newIndex);
        if (nodeState.hasType(IDs::PROCESSOR)) {
            NodeID nodeId = getNodeIdFromProcessorState(nodeState);
            removeNodeConnections(nodeId, parent, findNeighborNodes(parent, parent.getChild(oldIndex), parent.getChild(oldIndex + 1)));
            insertNodeConnections(nodeId, nodeState);
        }
    }

    void valueTreeParentChanged (ValueTree& treeWhoseParentHasChanged) override {}

    void valueTreeRedirected (ValueTree& treeWhichHasBeenChanged) override {}
};
