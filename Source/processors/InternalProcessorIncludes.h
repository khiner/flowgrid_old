#pragma once

#include "Arpeggiator.h"
#include "BalanceProcessor.h"
#include "GainProcessor.h"
#include "MidiInputProcessor.h"
#include "MidiKeyboardProcessor.h"
#include "MidiOutputProcessor.h"
#include "MixerChannelProcessor.h"
#include "ParameterTypesTestProcessor.h"
#include "SineBank.h"
#include "SineSynth.h"

Array<PluginDescription> internalPluginDescriptions {
        Arpeggiator::getPluginDescription(),
        BalanceProcessor::getPluginDescription(),
        GainProcessor::getPluginDescription(),
        MidiInputProcessor::getPluginDescription(),
        MidiKeyboardProcessor::getPluginDescription(),
        MidiOutputProcessor::getPluginDescription(),
        MixerChannelProcessor::getPluginDescription(),
        ParameterTypesTestProcessor::getPluginDescription(),
        SineBank::getPluginDescription(),
        SineSynth::getPluginDescription(),
};
