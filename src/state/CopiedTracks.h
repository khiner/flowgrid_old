#pragma once

#include "TracksState.h"
#include "ProcessorGraph.h"

struct CopiedTracks : public Stateful<CopiedTracks> {
    CopiedTracks(TracksState &tracks, ProcessorGraph &processorGraph)
            : tracks(tracks), processorGraph(processorGraph) {}

    static Identifier getIdentifier() { return TracksStateIDs::TRACKS; }

    void loadFromState(const ValueTree &fromState) override;

    void copySelectedItems() {
        loadFromState(tracks.getState());
    }

private:
    TracksState &tracks;
    ProcessorGraph &processorGraph;
};
