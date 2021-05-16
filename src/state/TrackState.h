#pragma once

#include "Stateful.h"
#include "ProcessorState.h"

namespace TrackStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(TRACK)
#undef ID
}

class TrackState : public Stateful<TrackState> {
public:
    static Identifier getIdentifier() { return TrackStateIDs::TRACK; }
};
