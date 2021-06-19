#pragma once

#include "Select.h"

struct SelectTrack : public Select {
    SelectTrack(const Track *, bool selected, bool deselectOthers, Tracks &, Connections &, View &, Input &, AllProcessors &, ProcessorGraph &);
};
