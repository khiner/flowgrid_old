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

    static String getUuid(const ValueTree &state) { return state[TrackIDs::uuid]; }
    static void setUuid(ValueTree &state, const String &uuid) { state.setProperty(TrackIDs::uuid, uuid, nullptr); }
    static String getName(const ValueTree &state) { return state[TrackIDs::name]; }
    static void setName(ValueTree &state, const String &name) { state.setProperty(TrackIDs::name, name, nullptr); }
    static Colour getColour(const ValueTree &state) { return Colour::fromString(state[TrackIDs::colour].toString()); }
    static void setColour(ValueTree &state, const Colour &colour) { state.setProperty(TrackIDs::colour, colour.toString(), nullptr); }
    static bool isSelected(const ValueTree &state) { return state[TrackIDs::selected]; }
    static void setSelected(ValueTree &state, bool selected) { state.setProperty(TrackIDs::selected, selected, nullptr); }
    static bool isMaster(const ValueTree &state) { return state[TrackIDs::isMasterTrack]; }
    static void setMaster(ValueTree &state, bool isMaster) { state.setProperty(TrackIDs::isMasterTrack, isMaster, nullptr); }

    static Colour getDisplayColour(const ValueTree &state) {
        return isSelected(state) ? getColour(state).brighter(0.25) : getColour(state);
    }
};
