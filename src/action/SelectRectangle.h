#pragma once

#include "Select.h"

struct SelectRectangle : public Select {
    SelectRectangle(const juce::Point<int> fromTrackAndSlot, const juce::Point<int> toTrackAndSlot,
                    Tracks &tracks, Connections &connections, View &view,
                    Input &input, ProcessorGraph &processorGraph);
};
