#pragma once

#include "Select.h"

struct SelectProcessorSlot : public Select {
    SelectProcessorSlot(const Track *, int slot, bool selected, bool deselectOthers, Tracks &, Connections &, View &, Input &, AllProcessors &, ProcessorGraph &);
};
