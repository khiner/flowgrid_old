#include "ProcessorGraph.h"

void ProcessorGraph::addProcessor(const ValueTree &processorState) {
    static String errorMessage = "Could not create processor";
    auto description = pluginManager.getDescriptionForIdentifier(processorState[IDs::id]);
    auto processor = pluginManager.getFormatManager().createPluginInstance(*description, getSampleRate(), getBlockSize(), errorMessage);
    if (processorState.hasProperty(IDs::state)) {
        MemoryBlock memoryBlock;
        memoryBlock.fromBase64Encoding(processorState[IDs::state].toString());
        processor->setStateInformation(memoryBlock.getData(), (int) memoryBlock.getSize());
    }

    const Node::Ptr &newNode = processorState.hasProperty(IDs::nodeId) ?
                               addNode(std::move(processor), getNodeIdForState(processorState)) :
                               addNode(std::move(processor));
    processorWrapperForNodeId[newNode->nodeID] = std::make_unique<StatefulAudioProcessorWrapper>
            (dynamic_cast<AudioPluginInstance *>(newNode->getProcessor()), newNode->nodeID, processorState, undoManager, project.getDeviceManager());
    if (processorWrapperForNodeId.size() == 1)
        // Added the first processor. Start the timer that flushes new processor state to their value trees.
        startTimerHz(10);

    if (auto midiInputProcessor = dynamic_cast<MidiInputProcessor *>(newNode->getProcessor())) {
        const String &deviceName = processorState.getProperty(IDs::deviceName);
        midiInputProcessor->setDeviceName(deviceName);
        if (deviceName.containsIgnoreCase(push2MidiDeviceName)) {
            push2MidiCommunicator.addMidiInputCallback(&midiInputProcessor->getMidiMessageCollector());
        } else {
            deviceManager.addMidiInputCallback(deviceName, &midiInputProcessor->getMidiMessageCollector());
        }
    } else if (auto *midiOutputProcessor = dynamic_cast<MidiOutputProcessor *>(newNode->getProcessor())) {
        const String &deviceName = processorState.getProperty(IDs::deviceName);
        if (auto *enabledMidiOutput = deviceManager.getEnabledMidiOutput(deviceName))
            midiOutputProcessor->setMidiOutput(enabledMidiOutput);
    }
    ValueTree mutableProcessor = processorState;
    if (mutableProcessor.hasProperty(IDs::processorInitialized))
        mutableProcessor.sendPropertyChangeMessage(IDs::processorInitialized);
    else
        mutableProcessor.setProperty(IDs::processorInitialized, true, nullptr);

}

void ProcessorGraph::recursivelyAddProcessors(const ValueTree &state) {
    if (state.hasType(IDs::PROCESSOR)) {
        addProcessor(state);
        return;
    }
    for (const ValueTree &child : state)
        recursivelyAddProcessors(child);
}

void ProcessorGraph::removeProcessor(const ValueTree &processor) {
    auto *processorWrapper = getProcessorWrapperForState(processor);
    const NodeID nodeId = getNodeIdForState(processor);

    // disconnect should have already been called before delete! (to avoid nested undo actions)
    if (processor[IDs::name] == MidiInputProcessor::name()) {
        if (auto *midiInputProcessor = dynamic_cast<MidiInputProcessor *>(processorWrapper->processor)) {
            const String &deviceName = processor.getProperty(IDs::deviceName);
            if (deviceName.containsIgnoreCase(push2MidiDeviceName)) {
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

void ProcessorGraph::updateIoChannelEnabled(const ValueTree &channels, const ValueTree &channel, bool enabled) {
    String processorName = channels.getParent()[IDs::name];
    bool isInput;
    if (processorName == "Audio Input" && channels.hasType(IDs::OUTPUT_CHANNELS))
        isInput = true;
    else if (processorName == "Audio Output" && channels.hasType(IDs::INPUT_CHANNELS))
        isInput = false;
    else
        return;
    if (auto *audioDevice = deviceManager.getCurrentAudioDevice()) {
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        auto &configChannels = isInput ? config.inputChannels : config.outputChannels;
        const auto &channelNames = isInput ? audioDevice->getInputChannelNames() : audioDevice->getOutputChannelNames();
        auto channelIndex = channelNames.indexOf(channel[IDs::name].toString());
        if (channelIndex != -1 && ((enabled && !configChannels[channelIndex]) || (!enabled && configChannels[channelIndex]))) {
            configChannels.setBit(channelIndex, enabled);
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
}
