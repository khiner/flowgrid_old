#pragma once

#include "Select.h"

struct SelectProcessorSlot : public Select {
    SelectProcessorSlot(const ValueTree &track, int slot, bool selected, bool deselectOthers, Tracks &tracks, Connections &connections, View &view, Input &input, ProcessorGraph &processorGraph);
};
