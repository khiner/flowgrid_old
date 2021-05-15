#pragma once

#include "Stateful.h"

namespace ConnectionStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(CONNECTION)
#undef ID
}

class ConnectionState : public Stateful, private ValueTree::Listener {
public:
    ConnectionState() = default;

    ~ConnectionState() override = 0;

    ValueTree &getState() override { return state; }

    static bool isType(const ValueTree &state) { return state.hasType(ConnectionStateIDs::CONNECTION); }

    void loadFromState(const ValueTree &fromState) override;

    void loadFromParentState(const ValueTree &parent) override { loadFromState(parent.getChildWithName(ConnectionStateIDs::CONNECTION)); }

private:
    ValueTree state;
};
