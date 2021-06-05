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

struct ProcessorGraph : public AudioProcessorGraph, private ValueTree::Listener, private Timer, private Tracks::Listener, private Output::Listener, private Input::Listener {
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

    bool disconnectProcessor(const Processor *processor) {
        undoManager.beginNewTransaction();
        return doDisconnectNode(processor, all, true, true, true, true);
    }

    bool disconnectNode(NodeID nodeId) override {
        return disconnectProcessor(processorWrappers.getProcessorForNodeId(nodeId));
    }

    bool doDisconnectNode(const Processor *processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, NodeID excludingRemovalTo = {});

    Node *getNodeForState(const ValueTree &processorState) const {
        return getNodeForId(Processor::getNodeId(processorState));
    }

    void onProcessorCreated(Processor *processor) {
        if (processorWrappers.getProcessorWrapperForProcessor(processor) == nullptr)
            addProcessor(processor);
    }

    void onProcessorDestroyed(const Processor *processor) {
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

    void addProcessor(Processor *processor);
    void removeProcessor(const Processor *processor);
    void recursivelyAddProcessors(const ValueTree &state);
    bool canAddConnection(Node *source, int sourceChannel, Node *dest, int destChannel);
    bool hasConnectionMatching(const Connection &connection);
    void updateIoChannelEnabled(const ValueTree &channels, const ValueTree &channel, bool enabled);

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;
    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) override;

    void trackAdded(Track *track) override {}
    void trackRemoved(Track *track, int oldIndex) override {}
    void processorAdded(Processor *processor) override { onProcessorCreated(processor); }
    void processorRemoved(Processor *processor, int oldIndex) override { onProcessorDestroyed(processor); }

    void timerCallback() override;
};
