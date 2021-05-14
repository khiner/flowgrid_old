#pragma once

#include "SelectAction.h"

struct SelectRectangleAction : public SelectAction {
    SelectRectangleAction(const juce::Point<int> fromTrackAndSlot, const juce::Point<int> toTrackAndSlot,
                          TracksState &tracks, ConnectionsState &connections, ViewState &view,
                          InputState &input, ProcessorGraph &processorGraph);
};
