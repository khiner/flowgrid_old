#pragma once

#include "Stateful.h"

namespace InputChannelsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(INPUT_CHANNELS)
#undef ID
}

class InputChannelsState : public Stateful<InputChannelsState> {
public:
    static Identifier getIdentifier() { return InputChannelsStateIDs::INPUT_CHANNELS; }
};
