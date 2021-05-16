#pragma once

#include "Stateful.h"
#include "ChannelState.h"

namespace OutputChannelsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(OUTPUT_CHANNELS)
#undef ID
}

class OutputChannelsState : public Stateful<OutputChannelsState>, private ValueTree::Listener {
public:
    static Identifier getIdentifier() { return OutputChannelsStateIDs::OUTPUT_CHANNELS; }
};
