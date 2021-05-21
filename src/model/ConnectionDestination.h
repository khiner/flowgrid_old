#pragma once

#include "Stateful.h"

namespace ConnectionDestinationIDs {
#define ID(name) const juce::Identifier name(#name);
ID(DESTINATION)
#undef ID
}

struct ConnectionDestination : public Stateful<ConnectionDestination> {
    static Identifier getIdentifier() { return ConnectionDestinationIDs::DESTINATION; }
};
