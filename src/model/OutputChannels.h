#pragma once

#include "Stateful.h"
#include "Channel.h"

namespace OutputChannelsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(OUTPUT_CHANNELS)
#undef ID
}

class OutputChannels : public Stateful<OutputChannels>, private ValueTree::Listener {
public:
    static Identifier getIdentifier() { return OutputChannelsIDs::OUTPUT_CHANNELS; }
};
