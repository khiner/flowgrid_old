#pragma once

#include "Select.h"

struct SelectTrack : public Select {
    SelectTrack(const ValueTree &track, bool selected, bool deselectOthers,
                Tracks &tracks, Connections &connections, View &view,
                Input &input, ProcessorGraph &processorGraph);
};
