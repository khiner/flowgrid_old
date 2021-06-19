#include "Processor.h"
#include "DeviceManagerUtilities.h"

Processor::Channel::Channel(AudioProcessor *processor, AudioDeviceManager &deviceManager, int channelIndex, bool isInput) :
        channelIndex(channelIndex) {
    if (processor->getName() == "Audio Input" || processor->getName() == "Audio Output") {
        name = DeviceManagerUtilities::getAudioChannelName(deviceManager, channelIndex, processor->getName() == "Audio Input");
        abbreviatedName = name;
    } else {
        if (channelIndex == AudioProcessorGraph::midiChannelIndex) {
            name = isInput ? "MIDI Input" : "MIDI Output";
            abbreviatedName = isInput ? "MIDI In" : "MIDI Out";
        } else {
            int busIndex = 0;
            auto channel = processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIndex);
            if (auto *bus = processor->getBus(isInput, busIndex)) {
                abbreviatedName = AudioChannelSet::getAbbreviatedChannelTypeName(bus->getCurrentLayout().getTypeOfChannel(channel));
                name = bus->getName() + ": " + abbreviatedName;
            } else {
                name = (isInput ? "Main Input: " : "Main Output: ") + String(channelIndex + 1);
                abbreviatedName = (isInput ? "Main In: " : "Main Out: ") + String(channelIndex + 1);
            }
        }
    }
}

Processor::Channel::Channel(const ValueTree &state) :
        channelIndex(fg::Channel::getChannelIndex(state)),
        name(fg::Channel::getName(state)),
        abbreviatedName(fg::Channel::getAbbreviatedName(state)) {
}

void Processor::loadFromState(const ValueTree &fromState) {
    Stateful<Processor>::loadFromState(fromState);
    resetVarToInt(state, ProcessorIDs::slot, nullptr);
    resetVarToInt(state, ProcessorIDs::nodeId, nullptr);
    resetVarToInt(state, ProcessorIDs::initialized, nullptr);
    resetVarToBool(state, ProcessorIDs::bypassed, nullptr);
    resetVarToBool(state, ProcessorIDs::acceptsMidi, nullptr);
    resetVarToBool(state, ProcessorIDs::producesMidi, nullptr);
    resetVarToBool(state, ProcessorIDs::allowDefaultConnections, nullptr);
}

void Processor::audioProcessorChanged(AudioProcessor *processor, const AudioProcessorListener::ChangeDetails &details) {
    if (processor == nullptr) return;

    if (MessageManager::getInstance()->isThisTheMessageThread())
        doAudioProcessorChanged(processor);
    else
        MessageManager::callAsync([this, processor] { doAudioProcessorChanged(processor); });
}

void Processor::doAudioProcessorChanged(AudioProcessor *audioProcessor) {
    Array<Channel> newInputs, newOutputs;
    for (int i = 0; i < audioProcessor->getTotalNumInputChannels(); i++)
        newInputs.add({audioProcessor, deviceManager, i, true});
    if (audioProcessor->acceptsMidi())
        newInputs.add({audioProcessor, deviceManager, AudioProcessorGraph::midiChannelIndex, true});
    for (int i = 0; i < audioProcessor->getTotalNumOutputChannels(); i++)
        newOutputs.add({audioProcessor, deviceManager, i, false});
    if (audioProcessor->producesMidi())
        newOutputs.add({audioProcessor, deviceManager, AudioProcessorGraph::midiChannelIndex, false});

    ValueTree inputChannels = getState().getChildWithName(InputChannelsIDs::INPUT_CHANNELS);
    ValueTree outputChannels = getState().getChildWithName(OutputChannelsIDs::OUTPUT_CHANNELS);
    if (!inputChannels.isValid()) {
        inputChannels = ValueTree(InputChannelsIDs::INPUT_CHANNELS);
        getState().appendChild(inputChannels, nullptr);
    }
    if (!outputChannels.isValid()) {
        outputChannels = ValueTree(OutputChannelsIDs::OUTPUT_CHANNELS);
        getState().appendChild(outputChannels, nullptr);
    }

    Array<Channel> oldInputs, oldOutputs;
    for (int i = 0; i < inputChannels.getNumChildren(); i++) {
        const auto &channel = inputChannels.getChild(i);
        oldInputs.add({channel});
    }
    for (int i = 0; i < outputChannels.getNumChildren(); i++) {
        const auto &channel = outputChannels.getChild(i);
        oldOutputs.add({channel});
    }

    if (audioProcessor->acceptsMidi())
        setAcceptsMidi(true);
    if (audioProcessor->producesMidi())
        setProducesMidi(true);

    updateChannels(oldInputs, newInputs, inputChannels);
    updateChannels(oldOutputs, newOutputs, outputChannels);
}

void Processor::updateChannels(Array<Channel> &oldChannels, Array<Channel> &newChannels, ValueTree &channelsState) {
    for (int i = 0; i < oldChannels.size(); i++) {
        const auto &oldChannel = oldChannels.getUnchecked(i);
        if (!newChannels.contains(oldChannel)) {
            channelsState.removeChild(channelsState.getChildWithProperty(ChannelIDs::name, oldChannel.name), &undoManager);
        }
    }
    for (int i = 0; i < newChannels.size(); i++) {
        const auto &newChannel = newChannels.getUnchecked(i);
        if (!oldChannels.contains(newChannel)) {
            channelsState.addChild(newChannel.toState(), i, &undoManager);
        }
    }
}
