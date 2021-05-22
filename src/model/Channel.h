#pragma once

#include "Stateful.h"

// Next up
namespace ChannelIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CHANNEL)
ID(channelIndex)
ID(name)
ID(abbreviatedName)
#undef ID
}

namespace fg {
struct Channel : public Stateful<Channel> {
    static Identifier getIdentifier() { return ChannelIDs::CHANNEL; }

    static int getChannelIndex(const ValueTree &state) { return state[ChannelIDs::channelIndex]; }
    static String getName(const ValueTree &state) { return state[ChannelIDs::name]; }
    static String getAbbreviatedName(const ValueTree &state) { return state[ChannelIDs::abbreviatedName]; }
    static void setChannelIndex(ValueTree &state, int channelIndex) { state.setProperty(ChannelIDs::channelIndex, channelIndex, nullptr); }
    static void setName(ValueTree &state, const String &name) { state.setProperty(ChannelIDs::name, name, nullptr); }
    static void setAbbreviatedName(ValueTree &state, const String &abbreviatedName) { state.setProperty(ChannelIDs::abbreviatedName, abbreviatedName, nullptr); }
};
}
