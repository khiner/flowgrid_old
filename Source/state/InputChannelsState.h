#pragma once

#include "Stateful.h"

namespace InputChannelsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(INPUT_CHANNELS)
#undef ID
}

class InputChannelsState : public Stateful<InputChannelsState>, private ValueTree::Listener {
public:
    ~InputChannelsState() override = 0;

    static Identifier getIdentifier() { return InputChannelsStateIDs::INPUT_CHANNELS; }
};
