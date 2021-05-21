#pragma once

#include "Stateful.h"

namespace ConnectionSourceIDs {
#define ID(name) const juce::Identifier name(#name);
ID(SOURCE)
#undef ID
}

struct ConnectionSource : public Stateful<ConnectionSource> {
    static Identifier getIdentifier() { return ConnectionSourceIDs::SOURCE; }
};
