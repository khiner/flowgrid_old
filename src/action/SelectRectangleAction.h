#pragma once

#include "SelectAction.h"

struct SelectRectangleAction : public SelectAction {
    SelectRectangleAction(const juce::Point<int> fromTrackAndSlot, const juce::Point<int> toTrackAndSlot,
                          Tracks &tracks, Connections &connections, View &view,
                          Input &input, ProcessorGraph &processorGraph);
};
