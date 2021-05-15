#pragma once

#include "Stateful.h"

namespace OutputChannelsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(OUTPUT_CHANNELS)
#undef ID
}

class OutputChannelsState : public Stateful<OutputChannelsState>, private ValueTree::Listener {
public:
    ~OutputChannelsState() override = 0;

    static Identifier getIdentifier() { return OutputChannelsStateIDs::OUTPUT_CHANNELS; }
};
