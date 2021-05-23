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

    static String getId(const ValueTree &state) { return state[ParamIDs::id]; }
    static float getValue(const ValueTree &state, float defaultValue = 0.0f) { return state.getProperty(ParamIDs::value, defaultValue); }

    static void setId(ValueTree &state, const String &id) { state.setProperty(ParamIDs::id, id, nullptr); }
    static void setValue(ValueTree &state, float value) { state.setProperty(ParamIDs::value, value, nullptr); }
};
