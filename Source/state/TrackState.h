#pragma once

#include "Stateful.h"

namespace TrackStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(TRACK)
#undef ID
}

class TrackState : public Stateful, private ValueTree::Listener {
public:
    TrackState() = default;

    ~TrackState() override = 0;

    ValueTree &getState() override { return state; }

    static bool isType(const ValueTree &state) { return state.hasType(TrackStateIDs::TRACK); }

    void loadFromState(const ValueTree &fromState) override;

    void loadFromParentState(const ValueTree &parent) override { loadFromState(parent.getChildWithName(TrackStateIDs::TRACK)); }

private:
    ValueTree state;
};
