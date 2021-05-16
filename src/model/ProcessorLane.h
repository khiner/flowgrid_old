#pragma once

#include "Stateful.h"
#include "Processor.h"

namespace ProcessorLaneIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANE)
ID(selectedSlotsMask)
#undef ID
}


struct ProcessorLane : public Stateful<ProcessorLane> {
    void setSelectedSlots(const BigInteger& selectedSlots) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlots.toString(2), nullptr); }

    static Identifier getIdentifier() { return ProcessorLaneIDs::PROCESSOR_LANE; }
};
