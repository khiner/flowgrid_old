#include "ProcessorLanes.h"

ProcessorLanes::ProcessorLanes() : StatefulList<ProcessorLane>(state) {
    rebuildObjects();
    if (size() == 0) {
        auto lane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
        ProcessorLane::setSelectedSlotsMask(lane, BigInteger());
        this->parent.appendChild(lane, nullptr);
    }
}

ProcessorLanes::ProcessorLanes(const ValueTree &state) : Stateful<ProcessorLanes>(state), StatefulList<ProcessorLane>(state) {
    rebuildObjects();
    if (size() == 0) {
        auto lane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
        ProcessorLane::setSelectedSlotsMask(lane, BigInteger());
        this->parent.appendChild(lane, nullptr);
    }
}

void ProcessorLanes::loadFromState(const ValueTree &fromState) {
    Stateful<ProcessorLanes>::loadFromState(fromState);
}
