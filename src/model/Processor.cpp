#include "Processor.h"

void Processor::loadFromState(const ValueTree &fromState) {
    Stateful<Processor>::loadFromState(fromState);
    resetVarToInt(state, ProcessorIDs::slot, nullptr);
    resetVarToInt(state, ProcessorIDs::nodeId, nullptr);
    resetVarToInt(state, ProcessorIDs::initialized, nullptr);
    resetVarToBool(state, ProcessorIDs::bypassed, nullptr);
    resetVarToBool(state, ProcessorIDs::acceptsMidi, nullptr);
    resetVarToBool(state, ProcessorIDs::producesMidi, nullptr);
    resetVarToBool(state, ProcessorIDs::allowDefaultConnections, nullptr);
}
