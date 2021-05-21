#pragma once

#include <action/DeleteConnection.h>
#include "action/CreateOrDeleteConnections.h"
#include "push2/Push2MidiCommunicator.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "model/Input.h"
#include "model/Output.h"
#include "model/Connections.h"
#include "PluginManager.h"

using namespace fg; // Only to disambiguate `Connection` currently

class ProcessorGraph : public AudioProcessorGraph,
                       private ValueTree::Listener, private Timer {
public:
    explicit ProcessorGraph(PluginManager &pluginManager, Tracks &tracks, Connections &connections,
                            Input &input, Output &output, UndoManager &undoManager, AudioDeviceManager &deviceManager,
                            Push2MidiCommunicator &push2MidiCommunicator);

    ~ProcessorGraph() override;

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(NodeID nodeId) const {
        if (!nodeId.isValid()) return nullptr;

        auto nodeIdAndProcessorWrapper = processorWrapperForNodeId.find(nodeId);
        if (nodeIdAndProcessorWrapper == processorWrapperForNodeId.end()) return nullptr;

        return nodeIdAndProcessorWrapper->second.get();
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return processorState.isValid() ? getProcessorWrapperForNodeId(Processor::getNodeId(processorState)) : nullptr;
    }

    AudioProcessor *getAudioProcessorForState(const ValueTree &processorState) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState))
            return processorWrapper->processor;
        return {};
    }

    void saveProcessorStateInformationToState(ValueTree &processorState) const {
        if (auto *processorWrapper = getProcessorWrapperForState(processorState)) {
            MemoryBlock memoryBlock;
            if (auto *processor = processorWrapper->processor) {
                processor->getStateInformation(memoryBlock);
                processorState.setProperty(ProcessorIDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
        }
    }

    ValueTree copyProcessor(ValueTree &fromProcessor) const {
        saveProcessorStateInformationToState(fromProcessor);
        auto copiedProcessor = fromProcessor.createCopy();
        copiedProcessor.removeProperty(ProcessorIDs::nodeId, nullptr);
        return copiedProcessor;
    }

    ValueTree getProcessorStateForNodeId(NodeID nodeId) const {
        if (auto processorWrapper = getProcessorWrapperForNodeId(nodeId))
            return processorWrapper->state;
        return {};
    }

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
        return disconnectProcessor(getProcessorStateForNodeId(nodeId));
    }

    bool doDisconnectNode(const ValueTree &processor, ConnectionType connectionType,
                          bool defaults, bool custom, bool incoming, bool outgoing, NodeID excludingRemovalTo = {});

    Node *getNodeForState(const ValueTree &processorState) const {
        return getNodeForId(Processor::getNodeId(processorState));
    }

    void onProcessorCreated(const ValueTree &processor) {
        if (getProcessorWrapperForState(processor) == nullptr)
            addProcessor(processor);
    }

    void onProcessorDestroyed(const ValueTree &processor) {
        removeProcessor(processor);
    }

private:
    std::map<NodeID, std::unique_ptr<StatefulAudioProcessorWrapper> > processorWrapperForNodeId;

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

    void timerCallback() override;
};
