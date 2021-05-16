#pragma once

#include "Stateful.h"

namespace ConnectionIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CONNECTION)
#undef ID
}

namespace fg {
class Connection : public Stateful<Connection> {
public:
    static Identifier getIdentifier() { return ConnectionIDs::CONNECTION; }
};
}
