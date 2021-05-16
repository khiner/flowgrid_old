#pragma once

#include "Tracks.h"
#include "ProcessorGraph.h"

struct CopiedTracks : public Stateful<CopiedTracks> {
    CopiedTracks(Tracks &tracks, ProcessorGraph &processorGraph)
            : tracks(tracks), processorGraph(processorGraph) {}

    static Identifier getIdentifier() { return TracksIDs::TRACKS; }

    void loadFromState(const ValueTree &fromState) override;

    void copySelectedItems() {
        loadFromState(tracks.getState());
    }

private:
    Tracks &tracks;
    ProcessorGraph &processorGraph;
};
