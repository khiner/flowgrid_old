#pragma once

#include "Stateful.h"
#include "ProcessorLane.h"

namespace ProcessorLanesIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANES)
#undef ID
}


struct ProcessorLanes : public Stateful<ProcessorLanes> {
    static Identifier getIdentifier() { return ProcessorLanesIDs::PROCESSOR_LANES; }

    void addLane(ProcessorLane &processorLane) { state.appendChild(processorLane.getState(), nullptr); }
};
