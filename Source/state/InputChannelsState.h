#pragma once

#include "Stateful.h"

namespace InputChannelsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(INPUT_CHANNELS)
#undef ID
}

class InputChannelsState : public Stateful, private ValueTree::Listener {
public:
    InputChannelsState() = default;

    ~InputChannelsState() override = 0;

    ValueTree &getState() override { return state; }

    static bool isType(const ValueTree &state) { return state.hasType(InputChannelsStateIDs::INPUT_CHANNELS); }

    void loadFromState(const ValueTree &fromState) override;

    void loadFromParentState(const ValueTree &parent) override { loadFromState(parent.getChildWithName(InputChannelsStateIDs::INPUT_CHANNELS)); }

private:
    ValueTree state;
};
