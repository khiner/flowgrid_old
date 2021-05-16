#pragma once

#include "SelectAction.h"

struct SelectTrackAction : public SelectAction {
    SelectTrackAction(const ValueTree &track, bool selected, bool deselectOthers,
                      Tracks &tracks, Connections &connections, View &view,
                      Input &input, ProcessorGraph &processorGraph);
};
