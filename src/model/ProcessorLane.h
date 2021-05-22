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
    static Identifier getIdentifier() { return ProcessorLaneIDs::PROCESSOR_LANE; }

    static BigInteger getSelectedSlotsMask(const ValueTree &state) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(state[ProcessorLaneIDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    static void setSelectedSlotsMask(ValueTree &state, const BigInteger &selectedSlotsMask) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr); }
};
