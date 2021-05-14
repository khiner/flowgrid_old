#pragma once

#include "TracksState.h"
#include "ProcessorGraph.h"

struct CopiedState : public Stateful {
    CopiedState(TracksState &tracks, ProcessorGraph &processorGraph)
            : tracks(tracks), processorGraph(processorGraph) {}

    ValueTree &getState() override { return copiedItems; }

    void loadFromState(const ValueTree &tracksState) override;

    void copySelectedItems() {
        loadFromState(tracks.getState());
    }

private:
    TracksState &tracks;
    ProcessorGraph &processorGraph;

    ValueTree copiedItems;
};
