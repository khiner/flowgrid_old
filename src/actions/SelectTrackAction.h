#pragma once

#include "SelectAction.h"

struct SelectTrackAction : public SelectAction {
    SelectTrackAction(const ValueTree &track, bool selected, bool deselectOthers,
                      TracksState &tracks, ConnectionsState &connections, ViewState &view,
                      InputState &input, ProcessorGraph &processorGraph);
};
