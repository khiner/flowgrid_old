#pragma once

#include "Stateful.h"

namespace ConnectionStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(CONNECTION)
#undef ID
}

class ConnectionState : public Stateful<ConnectionState>, private ValueTree::Listener {
public:
    ~ConnectionState() override = 0;

    static Identifier getIdentifier() { return ConnectionStateIDs::CONNECTION; }
};
