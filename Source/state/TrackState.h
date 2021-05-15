#pragma once

#include "Stateful.h"

namespace TrackStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(TRACK)
#undef ID
}

class TrackState : public Stateful<TrackState>, private ValueTree::Listener {
public:
    TrackState() = default;

    ~TrackState() override = 0;

    static Identifier getIdentifier() { return TrackStateIDs::TRACK; }
};
