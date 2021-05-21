#pragma once

#include "Stateful.h"

namespace ParamIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PARAM)
ID(id)
ID(value)
#undef ID
}

struct Param : public Stateful<Param> {
    static Identifier getIdentifier() { return ParamIDs::PARAM; }
};
