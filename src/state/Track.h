#pragma once

#include "Stateful.h"
#include "ProcessorLanes.h"

namespace TrackIDs {
#define ID(name) const juce::Identifier name(#name);
ID(TRACK)
ID(uuid)
ID(colour)
ID(name)
ID(selected)
ID(isMasterTrack)
#undef ID
}

class Track : public Stateful<Track> {
public:
    static Identifier getIdentifier() { return TrackIDs::TRACK; }
};
