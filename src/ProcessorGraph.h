#pragma once

#include <action/DeleteConnection.h>
#include "action/CreateOrDeleteConnections.h"
#include "push2/Push2MidiCommunicator.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "model/Input.h"
#include "model/Output.h"
#include "model/Connections.h"
#include "model/StatefulAudioProcessorWrappers.h"
#include "PluginManager.h"

using namespace fg; // Only to disambiguate `Connection` currently

struct ProcessorGraph : public AudioProcessorGraph, private ValueTree::Listener, private Timer, private Tracks::Listener {
    explicit ProcessorGraph(PluginManager &pluginManager, Tracks &tracks, Connections &connections,
                            Input &input, Output &output, UndoManager &undoManager, AudioDeviceManager &deviceManager,
                            Push2MidiCommunicator &push2MidiCommunicator);

    ~ProcessorGraph() override;

    StatefulAudioProcessorWrappers &getProcessorWrappers() { return processorWrappers; }

    void pauseAudioGraphUpdates() { graphUpdatesArePaused = true; }
    void resumeAudioGraphUpdatesAndApplyDiffSincePause();

    bool canAddConnection(const Connection &connection);
    bool removeConnection(const Connection &connection) override;
    bool addConnection(const Connection &connection) override;

    bool disconnectProcessor(const ValueTree &processor) {
        undoManager.beginNewTransaction();
        return doDisconnectNode(processor, all, true, true, true, true);
    }

    bool disconnectNode(NodeID nodeId) override {
        return disconnectProcessor(processorWrappers.getProcessorStateForNodeId(nodeId));
    }

    bool doDisconnectNode(const ValueTree &processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, NodeID excludingRemovalTo = {});

    Node *getNodeForState(const ValueTree &processorState) const {
        return getNodeForId(Processor::getNodeId(processorState));
    }

    void onProcessorCreated(const ValueTree &processor) {
        if (processorWrappers.getProcessorWrapperForState(processor) == nullptr)
            addProcessor(processor);
    }

    void onProcessorDestroyed(const ValueTree &processor) {
        removeProcessor(processor);
    }

private:
    StatefulAudioProcessorWrappers processorWrappers;

    Tracks &tracks;
    Connections &connections;
    Input &input;
    Output &output;

    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;
    PluginManager &pluginManager;
    Push2MidiCommunicator &push2MidiCommunicator;

    bool graphUpdatesArePaused{false};

    CreateOrDeleteConnections connectionsSincePause{connections};

    void addProcessor(const ValueTree &processorState);
    void removeProcessor(const ValueTree &processor);
    void recursivelyAddProcessors(const ValueTree &state);
    bool canAddConnection(Node *source, int sourceChannel, Node *dest, int destChannel);
    bool hasConnectionMatching(const Connection &connection);
    void updateIoChannelEnabled(const ValueTree &channels, const ValueTree &channel, bool enabled);

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;
    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) override;

    void trackAdded(Track *track) override {
        recursivelyAddProcessors(track->getState()); // TODO might be a problem for moving tracks
    }
    void trackRemoved(Track *track, int oldIndex) override {}

    void timerCallback() override;
};
