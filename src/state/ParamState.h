#pragma once

#include "Stateful.h"

namespace ParamStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(PARAM)
    ID(id)
    ID(value)
#undef ID
}

class ParamState : public Stateful<ParamState> {
    static Identifier getIdentifier() { return ParamStateIDs::PARAM; }
};
