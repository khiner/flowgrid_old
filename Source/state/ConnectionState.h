#pragma once

#include "Stateful.h"

namespace ConnectionStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(CONNECTION)
#undef ID
}

class ConnectionState : public Stateful<ConnectionState> {
public:
    static Identifier getIdentifier() { return ConnectionStateIDs::CONNECTION; }
};
