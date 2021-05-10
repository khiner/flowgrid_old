#pragma once

#include <actions/CreateOrDeleteConnectionsAction.h>
#include <processors/StatefulAudioProcessorWrapper.h>
#include "state/Project.h"
#include "state/ConnectionsState.h"
#include "PluginManager.h"
#include "StatefulAudioProcessorContainer.h"
#include "push2/Push2MidiCommunicator.h"

class ProcessorGraph : public AudioProcessorGraph, public StatefulAudioProcessorContainer,
                       private ValueTree::Listener, private Timer {
public:
    explicit ProcessorGraph(Project &project, TracksState &tracks, ConnectionsState &connections,
                            InputState &input, OutputState &output, UndoManager &undoManager, AudioDeviceManager &deviceManager,
                            Push2MidiCommunicator &push2MidiCommunicator);

    ~ProcessorGraph() override;

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(NodeID nodeId) const override {
        if (!nodeId.isValid()) return nullptr;

        auto nodeIdAndProcessorWrapper = processorWrapperForNodeId.find(nodeId);
        if (nodeIdAndProcessorWrapper == processorWrapperForNodeId.end()) return nullptr;

        return nodeIdAndProcessorWrapper->second.get();
    }

    void pauseAudioGraphUpdates() override {
        graphUpdatesArePaused = true;
    }

    void resumeAudioGraphUpdatesAndApplyDiffSincePause() override;

    AudioProcessorGraph::Node *getNodeForId(AudioProcessorGraph::NodeID nodeId) const override {
        return AudioProcessorGraph::getNodeForId(nodeId);
    }

    bool removeConnection(const Connection &c) override {
        return project.removeConnection(c);
    }

    bool addConnection(const Connection &c) override {
        return project.addConnection(c);
    }

    bool disconnectNode(AudioProcessorGraph::NodeID nodeId) override {
        return project.disconnectProcessor(getProcessorStateForNodeId(nodeId));
    }

    Node *getNodeForState(const ValueTree &processorState) const {
        return AudioProcessorGraph::getNodeForId(getNodeIdForState(processorState));
    }

private:
    std::map<NodeID, std::unique_ptr<StatefulAudioProcessorWrapper> > processorWrapperForNodeId;

    Project &project;
    TracksState &tracks;
    ConnectionsState &connections;
    InputState &input;
    OutputState &output;

    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;
    PluginManager &pluginManager;
    Push2MidiCommunicator &push2MidiCommunicator;

    bool graphUpdatesArePaused{false};

    CreateOrDeleteConnectionsAction connectionsSincePause{connections};

    void addProcessor(const ValueTree &processorState);
    void removeProcessor(const ValueTree &processor);
    void recursivelyAddProcessors(const ValueTree &state);

    void updateIoChannelEnabled(const ValueTree &channels, const ValueTree &channel, bool enabled);

    void onProcessorCreated(const ValueTree &processor) override {
        if (getProcessorWrapperForState(processor) == nullptr)
            addProcessor(processor);
    }

    void onProcessorDestroyed(const ValueTree &processor) override {
        removeProcessor(processor);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;
    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) override;

    void timerCallback() override;
};
