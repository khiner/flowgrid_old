#pragma once

#include "Stateful.h"
#include "Processor.h"

namespace ProcessorLaneIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANE)
ID(selectedSlotsMask)
#undef ID
}


class ProcessorLane : public Stateful<ProcessorLane> {
    static Identifier getIdentifier() { return ProcessorLaneIDs::PROCESSOR_LANE; }
};
