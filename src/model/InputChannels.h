#pragma once

#include "Stateful.h"

namespace InputChannelsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(INPUT_CHANNELS)
#undef ID
}

struct InputChannels : public Stateful<InputChannels> {
    static Identifier getIdentifier() { return InputChannelsIDs::INPUT_CHANNELS; }
};
