#pragma once

#include "Stateful.h"

namespace InputChannelsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(INPUT_CHANNELS)
#undef ID
}

class InputChannels : public Stateful<InputChannels> {
public:
    static Identifier getIdentifier() { return InputChannelsIDs::INPUT_CHANNELS; }
};
