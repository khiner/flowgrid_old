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

struct Track : public Stateful<Track> {
    static Identifier getIdentifier() { return TrackIDs::TRACK; }

    bool isMaster() const { return state[TrackIDs::isMasterTrack]; }
    void setMaster(bool isMaster) { state.setProperty(TrackIDs::isMasterTrack, isMaster, nullptr); }

    bool isSelected() const { return state[TrackIDs::selected]; }
    void setSelected(bool isSelected) { state.setProperty(TrackIDs::selected, isSelected, nullptr); }

    String getUuid() const { return state[TrackIDs::uuid]; }
    void setUuid(const String &uuid) { state.setProperty(TrackIDs::uuid, uuid, nullptr); }

    String getName() const { return state[TrackIDs::name]; }
    void setName(const String &name) { state.setProperty(TrackIDs::name, name, nullptr); }

    String getColour() const { return state[TrackIDs::colour]; }
    void setColour(const String &colour) { state.setProperty(TrackIDs::colour, colour, nullptr); }

    void addLanes(ProcessorLanes &processorLanes) { state.appendChild(processorLanes.getState(), nullptr); }
};
