#pragma once

#include "Stateful.h"
#include "ProcessorState.h"

namespace ProcessorLaneStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(PROCESSOR_LANE)
    ID(selectedSlotsMask)
#undef ID
}


class ProcessorLaneState : public Stateful<ProcessorLaneState> {
    static Identifier getIdentifier() { return ProcessorLaneStateIDs::PROCESSOR_LANE; }
};
