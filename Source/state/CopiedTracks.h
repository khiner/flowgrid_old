#pragma once

#include "TracksState.h"
#include "ProcessorGraph.h"

struct CopiedTracks : public Stateful {
    CopiedTracks(TracksState &tracks, ProcessorGraph &processorGraph)
            : tracks(tracks), processorGraph(processorGraph) {}

    ValueTree &getState() override { return state; }

    static bool isType(const ValueTree &state) { return state.hasType(TracksStateIDs::TRACKS); }

    void loadFromState(const ValueTree &fromState) override;

    void loadFromParentState(const ValueTree &parent) override { loadFromState(parent.getChildWithName(TracksStateIDs::TRACKS)); }

    void copySelectedItems() {
        loadFromState(tracks.getState());
    }

private:
    ValueTree state;

    TracksState &tracks;
    ProcessorGraph &processorGraph;
};
