#pragma once

#include "model/Input.h"
#include "CreateOrDeleteConnectionsAction.h"
#include "ProcessorGraph.h"

// For both audio and midi connection types:
//   * Find the topmost effect processor (receiving audio/midi) in the focused track
//   * Connect external device inputs to its most-upstream connected processor (including itself) that doesn't already have incoming connections
// (Note that it is possible for the same focused track to have a default audio-input processor different
// from its default midi-input processor.)

struct ResetDefaultExternalInputConnectionsAction : public CreateOrDeleteConnectionsAction {
    ResetDefaultExternalInputConnectionsAction(Connections &connections, Tracks &tracks, Input &input, ProcessorGraph &processorGraph, ValueTree trackToTreatAsFocused = {});

private:
    ValueTree findTopmostEffectProcessor(const ValueTree &track, ConnectionType connectionType);

    // Find the upper-right-most effect processor that flows into the given processor
    // which doesn't already have incoming node connections.
    ValueTree findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType, Tracks &tracks, Input &input);

    bool isAvailableForExternalInput(const ValueTree &processor, ConnectionType connectionType, Input &input);

    bool areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId);
};
