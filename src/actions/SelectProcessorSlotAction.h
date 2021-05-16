#pragma once

#include "SelectAction.h"

struct SelectProcessorSlotAction : public SelectAction {
    SelectProcessorSlotAction(const ValueTree &track, int slot, bool selected, bool deselectOthers,
                              TracksState &tracks, ConnectionsState &connections, ViewState &view,
                              InputState &input, ProcessorGraph &processorGraph);
};
