#pragma once

#include "Stateful.h"
#include "ConnectionSource.h"
#include "ConnectionDestination.h"

namespace ConnectionIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CONNECTION)
ID(channel)
ID(isCustomConnection)
#undef ID
}

namespace fg {
struct Connection : public Stateful<Connection> {
    static Identifier getIdentifier() { return ConnectionIDs::CONNECTION; }

    static int getChannel(const ValueTree& state) { return state[ConnectionIDs::channel]; }
    static void setChannel(ValueTree& state, int channel) { state.setProperty(ConnectionIDs::channel, channel, nullptr); }
};
}
