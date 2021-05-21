#include "ProcessorGraph.h"
#include "push2/Push2MidiDevice.h"
#include "processors/MidiInputProcessor.h"
#include "processors/MidiOutputProcessor.h"
#include "action/CreateConnection.h"
#include "action/UpdateProcessorDefaultConnections.h"
#include "action/ResetDefaultExternalInputConnectionsAction.h"
#include "action/DisconnectProcessor.h"

ProcessorGraph::ProcessorGraph(PluginManager &pluginManager, Tracks &tracks, Connections &connections, Input &input, Output &output, UndoManager &undoManager, AudioDeviceManager &deviceManager,
                               Push2MidiCommunicator &push2MidiCommunicator)
        : tracks(tracks), connections(connections), input(input), output(output),
          undoManager(undoManager), deviceManager(deviceManager), pluginManager(pluginManager), push2MidiCommunicator(push2MidiCommunicator) {
    enableAllBuses();

    tracks.addListener(this);
    connections.addListener(this);
    input.addListener(this);
    output.addListener(this);
}

ProcessorGraph::~ProcessorGraph() {
    output.removeListener(this);
    input.removeListener(this);
    connections.removeListener(this);
    tracks.removeListener(this);
}

void ProcessorGraph::addProcessor(const ValueTree &processorState) {
    static String errorMessage = "Could not create processor";
    auto description = pluginManager.getDescriptionForIdentifier(Processor::getId(processorState));
    auto audioProcessor = pluginManager.getFormatManager().createPluginInstance(*description, getSampleRate(), getBlockSize(), errorMessage);
    if (Processor::hasProcessorState(processorState)) {
        MemoryBlock memoryBlock;
        memoryBlock.fromBase64Encoding(Processor::getProcessorState(processorState));
        audioProcessor->setStateInformation(memoryBlock.getData(), (int) memoryBlock.getSize());
    }

    const Node::Ptr &newNode = Processor::hasNodeId(processorState) ?
                               addNode(std::move(audioProcessor), Processor::getNodeId(processorState)) :
                               addNode(std::move(audioProcessor));
    processorWrapperForNodeId[newNode->nodeID] = std::make_unique<StatefulAudioProcessorWrapper>
            (dynamic_cast<AudioPluginInstance *>(newNode->getProcessor()), newNode->nodeID, processorState, undoManager, deviceManager);
    if (processorWrapperForNodeId.size() == 1)
        // Added the first processor. Start the timer that flushes new processor state to their value trees.
        startTimerHz(10);

    if (auto midiInputProcessor = dynamic_cast<MidiInputProcessor *>(newNode->getProcessor())) {
        const String &deviceName = Processor::getDeviceName(processorState);
        midiInputProcessor->setDeviceName(deviceName);
        if (deviceName.containsIgnoreCase(Push2MidiDevice::getDeviceName())) {
            push2MidiCommunicator.addMidiInputCallback(&midiInputProcessor->getMidiMessageCollector());
        } else {
            deviceManager.addMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
        }
    } else if (auto *midiOutputProcessor = dynamic_cast<MidiOutputProcessor *>(newNode->getProcessor())) {
        const String &deviceName = Processor::getDeviceName(processorState);
        if (auto *enabledMidiOutput = deviceManager.getEnabledMidiOutput(deviceName))
            midiOutputProcessor->setMidiOutput(enabledMidiOutput);
    }
    ValueTree mutableProcessor = processorState;
    if (mutableProcessor.hasProperty(ProcessorIDs::processorInitialized))
        mutableProcessor.sendPropertyChangeMessage(ProcessorIDs::processorInitialized);
    else
        mutableProcessor.setProperty(ProcessorIDs::processorInitialized, true, nullptr);

}

void ProcessorGraph::recursivelyAddProcessors(const ValueTree &processorState) {
    if (Processor::isType(processorState)) {
        addProcessor(processorState);
        return;
    }
    for (const ValueTree &child : processorState)
        recursivelyAddProcessors(child);
}

void ProcessorGraph::removeProcessor(const ValueTree &processor) {
    auto *processorWrapper = getProcessorWrapperForState(processor);
    const NodeID nodeId = Processor::getNodeId(processor);
    // disconnect should have already been called before delete! (to avoid nested undo actions)
    if (Processor::getName(processor) == MidiInputProcessor::name()) {
        if (auto *midiInputProcessor = dynamic_cast<MidiInputProcessor *>(processorWrapper->processor)) {
            const String &deviceName = Processor::getDeviceName(processor);
            if (deviceName.containsIgnoreCase(Push2MidiDevice::getDeviceName())) {
                push2MidiCommunicator.removeMidiInputCallback(&midiInputProcessor->getMidiMessageCollector());
            } else {
                deviceManager.removeMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
            }
        }
    }
    processorWrapperForNodeId.erase(nodeId);
    nodes.removeObject(AudioProcessorGraph::getNodeForId(nodeId));
    topologyChanged();
}

bool ProcessorGraph::canAddConnection(const Connection &c) {
    if (auto *source = getNodeForId(c.source.nodeID))
        if (auto *dest = getNodeForId(c.destination.nodeID))
            return canAddConnection(source, c.source.channelIndex, dest, c.destination.channelIndex);

    return false;
}

bool ProcessorGraph::canAddConnection(Node *source, int sourceChannel, Node *dest, int destChannel) {
    bool sourceIsMIDI = sourceChannel == AudioProcessorGraph::midiChannelIndex;
    bool destIsMIDI = destChannel == AudioProcessorGraph::midiChannelIndex;

    if (sourceChannel < 0
        || destChannel < 0
        || source == dest
        || sourceIsMIDI != destIsMIDI
        || (!sourceIsMIDI && sourceChannel >= source->getProcessor()->getTotalNumOutputChannels())
        || (sourceIsMIDI && !source->getProcessor()->producesMidi())
        || (!destIsMIDI && destChannel >= dest->getProcessor()->getTotalNumInputChannels())
        || (destIsMIDI && !dest->getProcessor()->acceptsMidi())) {
        return false;
    }

    return !hasConnectionMatching({{source->nodeID, sourceChannel},
                                   {dest->nodeID,   destChannel}});
}

bool ProcessorGraph::addConnection(const AudioProcessorGraph::Connection &connection) {
    undoManager.beginNewTransaction();
    ConnectionType connectionType = connection.source.isMIDI() ? midi : audio;
    const auto &sourceProcessor = getProcessorStateForNodeId(connection.source.nodeID);
    // disconnect default outgoing
    undoManager.perform(new DisconnectProcessor(connections, sourceProcessor, connectionType, true, false, false, true));
    if (undoManager.perform(new CreateConnection(connection, false, connections, *this))) {
        undoManager.perform(new ResetDefaultExternalInputConnectionsAction(connections, tracks, input, *this));
        return true;
    }
    return false;
}

bool ProcessorGraph::hasConnectionMatching(const Connection &connection) {
    for (const auto &connectionState : connections.getState())
        if (Processor::toProcessorGraphConnection(connectionState) == connection)
            return true;
    return false;
}

bool ProcessorGraph::removeConnection(const AudioProcessorGraph::Connection &connection) {
    const ValueTree &connectionState = connections.getConnectionMatching(connection);
    // TODO add back isShiftHeld after refactoring key/midi event tracking into a non-project class
//    if (!connectionState[IDs::isCustomConnection] && isShiftHeld())
//        return false; // no default connection stuff while shift is held

    undoManager.beginNewTransaction();
    bool removed = undoManager.perform(new DeleteConnection(connectionState, true, true, connections));
    if (removed && connectionState.hasProperty(ConnectionIDs::isCustomConnection)) {
        const auto &source = connectionState.getChildWithName(ConnectionSourceIDs::SOURCE);
        const auto &sourceProcessor = getProcessorStateForNodeId(Processor::getNodeId(source));
        undoManager.perform(new UpdateProcessorDefaultConnections(sourceProcessor, false, connections, output, *this));
        undoManager.perform(new ResetDefaultExternalInputConnectionsAction(connections, tracks, input, *this));
    }
    return removed;
}

bool ProcessorGraph::doDisconnectNode(const ValueTree &processor, ConnectionType connectionType, bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo) {
    return undoManager.perform(new DisconnectProcessor(connections, processor, connectionType, defaults,
                                                       custom, incoming, outgoing, excludingRemovalTo));
}

void ProcessorGraph::updateIoChannelEnabled(const ValueTree &channels, const ValueTree &channel, bool enabled) {
    String processorName = channels.getParent()[ProcessorIDs::name];
    bool isInput;
    if (processorName == "Audio Input" && Output::isType(channels))
        isInput = true;
    else if (processorName == "Audio Output" && Input::isType(channels))
        isInput = false;
    else
        return;
    if (auto *audioDevice = deviceManager.getCurrentAudioDevice()) {
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        auto &configChannels = isInput ? config.inputChannels : config.outputChannels;
        const auto &channelNames = isInput ? audioDevice->getInputChannelNames() : audioDevice->getOutputChannelNames();
        auto channelIndex = channelNames.indexOf(channel[ChannelIDs::name].toString());
        if (channelIndex != -1 && ((enabled && !configChannels[channelIndex]) || (!enabled && configChannels[channelIndex]))) {
            configChannels.setBit(channelIndex, enabled);
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
}

void ProcessorGraph::resumeAudioGraphUpdatesAndApplyDiffSincePause() {
    graphUpdatesArePaused = false;
    for (auto &connection : connectionsSincePause.connectionsToDelete)
        valueTreeChildRemoved(connections.getState(), connection, 0);
    for (auto &connection : connectionsSincePause.connectionsToCreate)
        valueTreeChildAdded(connections.getState(), connection);
    connectionsSincePause.connectionsToDelete.clearQuick();
    connectionsSincePause.connectionsToCreate.clearQuick();
}

void ProcessorGraph::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (tree.hasType(ProcessorIDs::PROCESSOR)) {
        if (i == ProcessorIDs::bypassed) {
            if (auto node = getNodeForState(tree)) {
                node->setBypassed(tree[ProcessorIDs::bypassed]);
            }
        }
    }
}

void ProcessorGraph::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (fg::Connection::isType(child)) {
        if (graphUpdatesArePaused)
            connectionsSincePause.addConnection(child);
        else
            AudioProcessorGraph::addConnection(Processor::toProcessorGraphConnection(child));
    } else if (Track::isType(child) || Input::isType(parent) || Output::isType(parent)) {
        recursivelyAddProcessors(child); // TODO might be a problem for moving tracks
    } else if (Channel::isType(child)) {
        updateIoChannelEnabled(parent, child, true);
        // TODO shouldn't affect state in state listeners - trace back to specific user actions and do this in the action method
        removeIllegalConnections();
    }
}

void ProcessorGraph::valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) {
    if (fg::Connection::isType(child)) {
        if (graphUpdatesArePaused)
            connectionsSincePause.removeConnection(child);
        else
            AudioProcessorGraph::removeConnection(Processor::toProcessorGraphConnection(child));
    } else if (Channel::isType(child)) {
        updateIoChannelEnabled(parent, child, false);
        removeIllegalConnections(); // TODO shouldn't affect state in state listeners - trace back to specific user actions and do this in the action method
    }
}

void ProcessorGraph::timerCallback() {
    bool anythingUpdated = false;

    for (auto &nodeIdAndProcessorWrapper : processorWrapperForNodeId)
        if (nodeIdAndProcessorWrapper.second->flushParameterValuesToValueTree())
            anythingUpdated = true;

    startTimer(anythingUpdated ? 1000 / 50 : std::clamp(getTimerInterval() + 20, 50, 500));
}
