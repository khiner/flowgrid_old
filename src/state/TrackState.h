#pragma once

#include "Stateful.h"
#include "ProcessorState.h"

namespace TrackStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(TRACK)
    ID(uuid)
    ID(colour)
    ID(name)
    ID(selected)
    ID(isMasterTrack)
#undef ID
}

class TrackState : public Stateful<TrackState> {
public:
    static Identifier getIdentifier() { return TrackStateIDs::TRACK; }
};
