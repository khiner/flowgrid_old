#pragma once

#include "Stateful.h"
#include "Channel.h"

namespace OutputChannelsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(OUTPUT_CHANNELS)
#undef ID
}

struct OutputChannels : public Stateful<OutputChannels>, private ValueTree::Listener {
    static Identifier getIdentifier() { return OutputChannelsIDs::OUTPUT_CHANNELS; }
};
