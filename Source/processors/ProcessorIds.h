#pragma once

#include "JuceHeader.h"
#include "GainProcessor.h"
#include "MixerChannelProcessor.h"
#include "SineBank.h"
#include "BalanceProcessor.h"

const static StringArray processorIdsWithoutMixer { GainProcessor::name(), BalanceProcessor::name(), SineBank::name() };
static StringArray allProcessorIds { MixerChannelProcessor::name(), GainProcessor::name(), BalanceProcessor::name(), SineBank::name() };

static const StringArray getAvailableProcessorIdsForTrack(const ValueTree& track) {
    if (!track.isValid()) {
        return StringArray();
    } else if (track.getChildWithProperty(IDs::name, MixerChannelProcessor::name()).isValid()) {
        // at most one MixerChannel per track
        return processorIdsWithoutMixer;
    } else {
        return allProcessorIds;
    }
}

static StatefulAudioProcessor *createStatefulAudioProcessorFromId(const String &id, const ValueTree &state, UndoManager &undoManager) {
    if (id == MixerChannelProcessor::name()) {
        return new MixerChannelProcessor(state, undoManager);
    } else if (id == GainProcessor::name()) {
        return new GainProcessor(state, undoManager);
    } else if (id == BalanceProcessor::name()) {
        return new BalanceProcessor(state, undoManager);
    } else if (id == SineBank::name()) {
        return new SineBank(state, undoManager);
    } else {
        return nullptr;
    }
}