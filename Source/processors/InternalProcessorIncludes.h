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
#include "TrackInputProcessor.h"
#include "TrackOutputProcessor.h"

Array<PluginDescription> internalPluginDescriptions{
        TrackInputProcessor::getPluginDescription(),
        TrackOutputProcessor::getPluginDescription(),
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
