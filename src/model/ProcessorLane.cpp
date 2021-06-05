#include "ProcessorLane.h"

ProcessorLane::ProcessorLane() : StatefulList<Processor>(state) {
    rebuildObjects();
}

void ProcessorLane::loadFromState(const ValueTree &fromState) {
    Stateful<ProcessorLane>::loadFromState(fromState);
    // See note in Tracks::loadFromState
    parent.sendPropertyChangeMessage(ProcessorLaneIDs::selectedSlotsMask);
    for (auto processor : parent) {
        resetVarToInt(processor, ProcessorIDs::slot, nullptr);
        resetVarToInt(processor, ProcessorIDs::nodeId, nullptr);
        resetVarToInt(processor, ProcessorIDs::initialized, nullptr);
        resetVarToBool(processor, ProcessorIDs::bypassed, nullptr);
        resetVarToBool(processor, ProcessorIDs::acceptsMidi, nullptr);
        resetVarToBool(processor, ProcessorIDs::producesMidi, nullptr);
        resetVarToBool(processor, ProcessorIDs::allowDefaultConnections, nullptr);
    }
}
