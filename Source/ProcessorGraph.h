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

        this->project.getConnections().addListener(this);
        recursivelyInitializeWithState(project.getMasterTrack(), !hasConnections);
        recursivelyInitializeWithState(project.getTracks(), !hasConnections);

        if (hasConnections) {
            for (auto connection : project.getConnections()) {
                valueTreeChildAdded(project.getConnections(), connection);
            }
        }
        this->project.getTracks().addListener(this);
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
        const Point<int> &gridLocation = getProcessorGridLocation(nodeId);
        currentlyDraggingNodeId = nodeId;
        currentlyDraggingGridPosition.setXY(gridLocation.x, gridLocation.y);
        initialDraggingGridPosition = currentlyDraggingGridPosition;
        project.makeConnectionsSnapshot();
    }

    void setNodePosition(NodeID nodeId, Point<double> pos) {
        jassert(currentlyDraggingNodeId != NA_NODE_ID);

        auto newX = int(Project::NUM_VISIBLE_TRACK_SLOTS * jlimit(0.0, 0.99, pos.x));
        auto newY = int(Project::NUM_VISIBLE_TRACK_SLOTS * jlimit(0.0, 0.99, pos.y));
        if (newX != currentlyDraggingGridPosition.x || newY != currentlyDraggingGridPosition.y) {
            currentlyDraggingGridPosition.x = newX;
            currentlyDraggingGridPosition.y = newY;
            StatefulAudioProcessor *processor = getProcessorForNodeId(nodeId);
            moveProcessor(processor->state, initialDraggingGridPosition.x, initialDraggingGridPosition.y);
            project.restoreConnectionsSnapshot();
            if (currentlyDraggingGridPosition != initialDraggingGridPosition) {
                moveProcessor(processor->state, currentlyDraggingGridPosition.x, currentlyDraggingGridPosition.y);
            }
        }
    }

    void endDraggingNode(NodeID nodeId) {
        if (currentlyDraggingNodeId != NA_NODE_ID) {
            // update the audio graph to match the current preview UI graph.
            currentlyDraggingNodeId = NA_NODE_ID;
            project.applyConnectionDiffSinceSnapshot(this);
        }
    }

    void moveProcessor(ValueTree &processorState, int toTrackIndex, int toSlot) {
        processorState.setProperty(IDs::PROCESSOR_SLOT, toSlot, nullptr);
        const ValueTree &toTrack = project.getTrack(toTrackIndex);
        const int insertIndex = project.getParentIndexForProcessor(toTrack, processorState);
        processorState.setProperty(IDs::IS_MOVING, true, nullptr);
        Helpers::moveSingleItem(processorState, toTrack, insertIndex, currentlyDraggingNodeId == NA_NODE_ID ? &undoManager : nullptr);
        processorState.removeProperty(IDs::IS_MOVING, nullptr);
    }

    Point<double> getNodePosition(NodeID nodeId) const {
        const Point<int> gridLocation = getProcessorGridLocation(nodeId);
        return {gridLocation.x / float(Project::NUM_VISIBLE_TRACK_SLOTS) + (0.5 / Project::NUM_VISIBLE_TRACK_SLOTS),
                gridLocation.y / float(Project::NUM_VISIBLE_TRACK_SLOTS) + (0.5 / Project::NUM_VISIBLE_TRACK_SLOTS)};
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
            || (! sourceIsMIDI && sourceChannel >= source->processor->getTotalNumOutputChannels())
            || (sourceIsMIDI && ! source->processor->producesMidi()))
            return false;

        if (dest == nullptr
            || (! destIsMIDI && destChannel >= dest->processor->getTotalNumInputChannels())
            || (destIsMIDI && ! dest->processor->acceptsMidi()))
            return false;

        return !isConnectedUi({{source->nodeID, sourceChannel}, {dest->nodeID, destChannel}});
    }

    bool addConnection(const Connection& c) override {
        if (canConnectUi(c)) {
            project.addConnection(c);
            return true;
        }
        return false;
    }

    bool removeConnection(const Connection& c) override {
        return project.removeConnection(c);
    }

private:
    const static NodeID NA_NODE_ID = 0;

    NodeID currentlyDraggingNodeId = NA_NODE_ID;
    Point<int> currentlyDraggingGridPosition;
    Point<int> initialDraggingGridPosition;

    Project &project;
    UndoManager &undoManager;
    Node::Ptr audioOutputNode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

    void recursivelyInitializeWithState(const ValueTree &state, bool addDefaultConnections=true) {
        if (state.hasType(IDs::PROCESSOR)) {
            return addProcessor(state, addDefaultConnections);
        }
        for (const ValueTree& child : state) {
            if (!child.hasType(IDs::MASTER_TRACK)) {
                recursivelyInitializeWithState(child, addDefaultConnections);
            }
        }
    }

    void addProcessor(const ValueTree &processorState, bool addDefaultConnections=true) {
        auto *processor = createStatefulAudioProcessorFromId(processorState[IDs::name], processorState, undoManager);
        processor->updateValueTree();

        const Node::Ptr &newNode = processorState.hasProperty(IDs::NODE_ID) ?
                                   addNode(processor, getNodeIdForState(processorState)) :
                                   addNode(processor);
        processor->state.setProperty(IDs::NODE_ID, int(newNode->nodeID), nullptr);
        if (addDefaultConnections) {
            insertNodeConnections(newNode->nodeID, processorState);
        }
    }

    Point<int> getProcessorGridLocation(NodeID nodeId) const {
        if (nodeId == audioOutputNode->nodeID) {
            return {Project::NUM_VISIBLE_TRACK_SLOTS - 1, Project::NUM_VISIBLE_TRACK_SLOTS - 1};
        } else if (auto *processor = getProcessorForNodeId(nodeId)) {
            auto row = int(processor->state[IDs::PROCESSOR_SLOT]);
            auto column = processor->state.getParent().getParent().indexOf(processor->state.getParent());
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

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& property) override {}

    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (child.hasProperty(IDs::IS_MOVING)) {
                // processor and node already exist. no need to destroy and create again. just moving between tracks.
                insertNodeConnections(getNodeIdForState(child), child);
            } else {
                addProcessor(child);
            }
            project.makeSlotsValid(parent);

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
            }
            topologyChanged();
        }
    }

    void valueTreeChildRemoved(ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            NodeID removedNodeId = getNodeIdForState(child);

            removeNodeConnections(removedNodeId, parent, findNeighborNodes(parent, parent.getChild(indexFromWhichChildWasRemoved - 1), parent.getChild(indexFromWhichChildWasRemoved)));
            if (!child.hasProperty(IDs::IS_MOVING)) {
                // no need to destroy and create again. just moving between tracks.
                removeNode(removedNodeId);
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
            }
            topologyChanged();
        }
    }

    void valueTreeChildOrderChanged(ValueTree& parent, int oldIndex, int newIndex) override {
        ValueTree processorState = parent.getChild(newIndex);
        if (processorState.hasType(IDs::PROCESSOR)) {
            NodeID nodeId = getNodeIdForState(processorState);
            const NeighborNodes &neighborNodes = oldIndex < newIndex ?
                                                 findNeighborNodes(parent, parent.getChild(oldIndex - 1), parent.getChild(oldIndex)) :
                                                 findNeighborNodes(parent, parent.getChild(oldIndex), parent.getChild(oldIndex + 1));
            removeNodeConnections(nodeId, parent, neighborNodes);
            insertNodeConnections(nodeId, processorState);
            project.makeSlotsValid(parent);
        }
    }

    void valueTreeParentChanged(ValueTree& treeWhoseParentHasChanged) override {
    }

    void valueTreeRedirected(ValueTree& treeWhichHasBeenChanged) override {}
};
