#pragma once

#include "Stateful.h"

namespace OutputChannelsStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(OUTPUT_CHANNELS)
#undef ID
}

class OutputChannelsState : public Stateful, private ValueTree::Listener {
public:
    OutputChannelsState() = default;

    ~OutputChannelsState() override = 0;

    ValueTree &getState() override { return state; }

    static bool isType(const ValueTree &state) { return state.hasType(OutputChannelsStateIDs::OUTPUT_CHANNELS); }

    void loadFromState(const ValueTree &fromState) override;

    void loadFromParentState(const ValueTree &parent) override { loadFromState(parent.getChildWithName(OutputChannelsStateIDs::OUTPUT_CHANNELS)); }

private:
    ValueTree state;
};
