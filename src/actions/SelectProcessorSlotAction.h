#pragma once

#include "SelectAction.h"

struct SelectProcessorSlotAction : public SelectAction {
    SelectProcessorSlotAction(const ValueTree &track, int slot, bool selected, bool deselectOthers,
                              Tracks &tracks, Connections &connections, View &view,
                              Input &input, ProcessorGraph &processorGraph);
};
