#pragma once

#include <actions/CreateOrDeleteConnectionsAction.h>
#include <processors/StatefulAudioProcessorWrapper.h>
#include <state/Identifiers.h>
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
                            Push2MidiCommunicator &push2MidiCommunicator)
            : project(project), tracks(tracks), connections(connections), input(input), output(output),
              undoManager(undoManager), deviceManager(deviceManager), pluginManager(project.getPluginManager()), push2MidiCommunicator(push2MidiCommunicator) {
        enableAllBuses();

        tracks.addListener(this);
        connections.addListener(this);
        input.addListener(this);
        output.addListener(this);
        project.setStatefulAudioProcessorContainer(this);
    }

    ~ProcessorGraph() override {
        project.setStatefulAudioProcessorContainer(nullptr);
        output.removeListener(this);
        input.removeListener(this);
        connections.removeListener(this);
        tracks.removeListener(this);
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(NodeID nodeId) const override {
        if (!nodeId.isValid()) return nullptr;

        auto nodeIdAndProcessorWrapper = processorWrapperForNodeId.find(nodeId);
        if (nodeIdAndProcessorWrapper == processorWrapperForNodeId.end()) return nullptr;

        return nodeIdAndProcessorWrapper->second.get();
    }

    void pauseAudioGraphUpdates() override {
        graphUpdatesArePaused = true;
    }

    void resumeAudioGraphUpdatesAndApplyDiffSincePause() override {
        graphUpdatesArePaused = false;
        for (auto &connection : connectionsSincePause.connectionsToDelete)
            valueTreeChildRemoved(connections.getState(), connection, 0);
        for (auto &connection : connectionsSincePause.connectionsToCreate)
            valueTreeChildAdded(connections.getState(), connection);
        connectionsSincePause.connectionsToDelete.clearQuick();
        connectionsSincePause.connectionsToCreate.clearQuick();
    }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)

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

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::PROCESSOR)) {
            if (i == IDs::bypassed) {
                if (auto node = getNodeForState(tree)) {
                    node->setBypassed(tree[IDs::bypassed]);
                }
            }
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::CONNECTION)) {
            if (graphUpdatesArePaused)
                connectionsSincePause.addConnection(child);
            else
                AudioProcessorGraph::addConnection(ConnectionsState::stateToConnection(child));
        } else if (child.hasType(IDs::TRACK) || parent.hasType(IDs::INPUT) || parent.hasType(IDs::OUTPUT)) {
            recursivelyAddProcessors(child); // TODO might be a problem for moving tracks
        } else if (child.hasType(IDs::CHANNEL)) {
            updateIoChannelEnabled(parent, child, true);
            // TODO shouldn't affect state in state listeners - trace back to specific user actions and do this in the action method
            removeIllegalConnections();
        }
    }

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::CONNECTION)) {
            if (graphUpdatesArePaused)
                connectionsSincePause.removeConnection(child);
            else
                AudioProcessorGraph::removeConnection(ConnectionsState::stateToConnection(child));
        } else if (child.hasType(IDs::CHANNEL)) {
            updateIoChannelEnabled(parent, child, false);
            removeIllegalConnections(); // TODO shouldn't affect state in state listeners - trace back to specific user actions and do this in the action method
        }
    }

    void timerCallback() override {
        bool anythingUpdated = false;

        for (auto &nodeIdAndProcessorWrapper : processorWrapperForNodeId)
            if (nodeIdAndProcessorWrapper.second->flushParameterValuesToValueTree())
                anythingUpdated = true;

        startTimer(anythingUpdated ? 1000 / 50 : std::clamp(getTimerInterval() + 20, 50, 500));
    }
};
