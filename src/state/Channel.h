#pragma once

#include "Stateful.h"

namespace ChannelIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CHANNEL)
ID(channelIndex)
ID(name)
ID(abbreviatedName)
#undef ID
}

class Channel : public Stateful<Channel> {
    static Identifier getIdentifier() { return ChannelIDs::CHANNEL; }
};
