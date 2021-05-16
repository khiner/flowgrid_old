#pragma once

#include "Stateful.h"
#include "ProcessorLaneState.h"

namespace ProcessorLanesStateIDs {
#define ID(name) const juce::Identifier name(#name);
    ID(PROCESSOR_LANES)
#undef ID
}


class ProcessorLanesState : public Stateful<ProcessorLanesState> {
    static Identifier getIdentifier() { return ProcessorLanesStateIDs::PROCESSOR_LANES; }
};
