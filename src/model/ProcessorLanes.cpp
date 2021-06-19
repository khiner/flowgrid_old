#include "ProcessorLanes.h"

ProcessorLanes::ProcessorLanes(UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : StatefulList<ProcessorLane>(state), undoManager(undoManager), deviceManager(deviceManager) {
    rebuildObjects();
    if (size() == 0) {
        auto lane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
        ProcessorLane::setSelectedSlotsMask(lane, BigInteger());
        this->parent.appendChild(lane, nullptr);
    }
}

ProcessorLanes::ProcessorLanes(const ValueTree &state, UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : Stateful<ProcessorLanes>(state), StatefulList<ProcessorLane>(state), undoManager(undoManager), deviceManager(deviceManager) {
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
