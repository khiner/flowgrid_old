#pragma once

#include "Select.h"

struct SelectRectangle : public Select {
    SelectRectangle(const juce::Point<int> fromTrackAndSlot, const juce::Point<int> toTrackAndSlot, Tracks &, Connections &, View &, Input &, AllProcessors &, ProcessorGraph &);
};
