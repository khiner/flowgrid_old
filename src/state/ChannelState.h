#pragma once

#include "Stateful.h"

namespace ChannelStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(CHANNEL)
    ID(channelIndex)
    ID(abbreviatedName)
#undef ID
}

class ChannelState : public Stateful<ChannelState> {
    static Identifier getIdentifier() { return ChannelStateIDs::CHANNEL; }
};
