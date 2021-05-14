#pragma once

#include <actions/DeleteConnectionAction.h>
#include "actions/CreateOrDeleteConnectionsAction.h"
#include "push2/Push2MidiCommunicator.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "state/InputState.h"
#include "state/OutputState.h"
#include "state/ConnectionsState.h"
#include "PluginManager.h"

class ProcessorGraph : public AudioProcessorGraph,
                       private ValueTree::Listener, private Timer {
public:
    explicit ProcessorGraph(PluginManager &pluginManager, TracksState &tracks, ConnectionsState &connections,
                            InputState &input, OutputState &output, UndoManager &undoManager, AudioDeviceManager &deviceManager,
                            Push2MidiCommunicator &push2MidiCommunicator);

    ~ProcessorGraph() override;

    StatefulAudioProcessorWrapper *getProcessorWrapperForNodeId(NodeID nodeId) const {
        if (!nodeId.isValid()) return nullptr;

        auto nodeIdAndProcessorWrapper = processorWrapperForNodeId.find(nodeId);
        if (nodeIdAndProcessorWrapper == processorWrapperForNodeId.end()) return nullptr;

        return nodeIdAndProcessorWrapper->second.get();
    }

    StatefulAudioProcessorWrapper *getProcessorWrapperForState(const ValueTree &processorState) const {
        return processorState.isValid() ? getProcessorWrapperForNodeId(TracksState::getNodeIdForProcessor(processorState)) : nullptr;
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
                processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
        }
    }

    ValueTree copyProcessor(ValueTree &fromProcessor) const {
        saveProcessorStateInformationToState(fromProcessor);
        auto copiedProcessor = fromProcessor.createCopy();
        copiedProcessor.removeProperty(IDs::nodeId, nullptr);
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
        return getNodeForId(TracksState::getNodeIdForProcessor(processorState));
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

    bool canAddConnection(Node *source, int sourceChannel, Node *dest, int destChannel);

    bool hasConnectionMatching(const Connection &connection);

    void updateIoChannelEnabled(const ValueTree &channels, const ValueTree &channel, bool enabled);

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) override;

    void timerCallback() override;
};
