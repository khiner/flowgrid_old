#pragma once

#include <Identifiers.h>

#include "JuceHeader.h"
#include <processors/SineBank.h>
#include <processors/GainProcessor.h>

static StatefulAudioProcessor *createStatefulAudioProcessorFromId(const String &id, const ValueTree &state, UndoManager &undoManager) {
    if (id == SineBank::name()) {
        return new SineBank(state, undoManager);
    } else if (id == GainProcessor::name()) {
        return new GainProcessor(state, undoManager);
    } else {
        return nullptr;
    }
}

class AudioGraphBuilder : public AudioProcessorGraph, private ValueTree::Listener {
public:
    explicit AudioGraphBuilder(const ValueTree &projectState, UndoManager &undoManager)
            : projectState(projectState), undoManager(undoManager) {
        this->projectState.addListener(this);
        enableAllBuses();
        audioOutputNode = addNode(new AudioGraphIOProcessor(AudioGraphIOProcessor::audioOutputNode));

        recursivelyInitializeWithState(getMasterTrack());
        recursivelyInitializeWithState(projectState);
    }

    StatefulAudioProcessor *getAudioProcessor(String &uuid) {
        auto nodeID = getNodeID(uuid);
        return nodeID != -1 ? dynamic_cast<StatefulAudioProcessor *>(getNodeForId(nodeID)->getProcessor()) : nullptr;
    }

    const ValueTree getMasterTrack() {
        return projectState.getChildWithName(IDs::MASTER_TRACK);
    }

    StatefulAudioProcessor *getMasterGainProcessor() {
        const ValueTree masterTrack = getMasterTrack();
        ValueTree gain = masterTrack.getChildWithProperty(IDs::name, GainProcessor::name());
        String uuid = gain[IDs::uuid];
        return getAudioProcessor(uuid);
    }

private:
    ValueTree projectState;
    UndoManager &undoManager;
    Node::Ptr audioOutputNode;
    std::unordered_map<String, NodeID> nodeIdForUuid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphBuilder)

    const NodeID getNodeID(String &uuid) {
        auto pair = nodeIdForUuid.find(uuid);
        return pair != nodeIdForUuid.end() ? pair->second : -1;
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
        String previousProcessorUuid = processorState.getSibling(-1)[IDs::uuid];
        NodeID oldNodeID = getNodeID(previousProcessorUuid);
        if (processorState.getParent().hasType(IDs::MASTER_TRACK)) {
            for (int channel = 0; channel < 2; ++channel) {
                if (oldNodeID != -1) {
                    removeConnection({{oldNodeID,      channel},
                                      {audioOutputNode->nodeID, channel}});
                    addConnection({{oldNodeID,                 channel},
                                   {newNode->nodeID, channel}});
                }
                addConnection({{newNode->nodeID,         channel},
                               {audioOutputNode->nodeID, channel}});
            }
        } else {
            String firstMasterProcessorUuid = projectState.getChildWithName(IDs::MASTER_TRACK).getChildWithName(IDs::PROCESSOR)[IDs::uuid];
            NodeID firstMasterTrackNodeID = getNodeID(firstMasterProcessorUuid);
            if (firstMasterTrackNodeID != -1) {
                for (int channel = 0; channel < 2; ++channel) {
                    if (oldNodeID != -1) {
                        removeConnection({{oldNodeID,              channel},
                                          {firstMasterTrackNodeID, channel}});
                        addConnection({{oldNodeID,                 channel},
                                       {newNode->nodeID, channel}});
                    }
                    addConnection({{newNode->nodeID,                 channel},
                                   {firstMasterTrackNodeID, channel}});
                }
            }
        }
        // Kind of crappy - the order of the listeners seems to be nondeterministic,
        // so send (maybe _another_) select message that will update the UI in case this was already selected.
        if (processorState[IDs::selected]) {
            processor->state.sendPropertyChangeMessage(IDs::selected);
        }
    }

    void valueTreePropertyChanged (ValueTree& tree, const Identifier& property) override {}

    void valueTreeChildAdded (ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            addProcessor(child);
        }
    }

    void valueTreeChildRemoved (ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            String uuid = child[IDs::uuid].toString();
            Node *removedNode = getNodeForId(getNodeID(uuid));

            removeNodeConnections(removedNode);

            nodeIdForUuid.erase(uuid);
            removeNode(removedNode);
        }
    }

    void valueTreeChildOrderChanged (ValueTree& parent, int oldIndex, int newIndex) override {
        ValueTree child = parent.getChild(newIndex);
        if (child.hasType(IDs::PROCESSOR)) {
            String uuid = child[IDs::uuid].toString();
            Node *node = getNodeForId(getNodeID(uuid));
            removeNodeConnections(node);
            insertNodeConnections(node, child);
        }
    }

    void valueTreeParentChanged (ValueTree& treeWhoseParentHasChanged) override {
        // TODO
    }

    void valueTreeRedirected (ValueTree& treeWhichHasBeenChanged) override {}

    void removeNodeConnections(Node *node) {
        for (auto& o : node->outputs) {
            int channel = o.thisChannel;
            jassert(o.otherChannel == channel);
            removeConnection({{node->nodeID,      channel},
                              {o.otherNode->nodeID, channel}});
            for (auto& i : node->inputs) {
                jassert(i.thisChannel == i.otherChannel);
                if (i.thisChannel == channel) {
                    addConnection({{i.otherNode->nodeID, channel},
                                   {o.otherNode->nodeID, channel}});
                }
            }
        }
    }

    void insertNodeConnections(Node *node, const ValueTree& child) {
        const ValueTree &before = child.getSibling(-1);
        const ValueTree &after = child.getSibling(1);

        if (before.isValid()) {
            String beforeUuid = before[IDs::uuid].toString();
            Node *beforeNode = getNodeForId(getNodeID(beforeUuid));

            for (auto& o : beforeNode->outputs) {
                int channel = o.thisChannel;
                jassert(o.otherChannel == channel);
                removeConnection({{beforeNode->nodeID,      channel},
                                  {o.otherNode->nodeID, channel}});
                addConnection({{beforeNode->nodeID,      channel},
                               {node->nodeID, channel}});
                addConnection({{node->nodeID,      channel},
                               {o.otherNode->nodeID, channel}});
            }
        } else if (after.isValid()) {
            String afterUuid = after[IDs::uuid].toString();
            Node *afterNode = getNodeForId(getNodeID(afterUuid));

            for (auto& o : afterNode->inputs) {
                int channel = o.thisChannel;
                jassert(o.otherChannel == channel);
                removeConnection({{o.otherNode->nodeID,      channel},
                                  {afterNode->nodeID, channel}});
                addConnection({{o.otherNode->nodeID,      channel},
                               {node->nodeID, channel}});
            }
            for (int channel = 0; channel < afterNode->getProcessor()->getTotalNumInputChannels(); channel++) {
                addConnection({{node->nodeID,      channel},
                               {afterNode->nodeID, channel}});
            }
        }
    }
};
