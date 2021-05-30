#include "Track.h"

int Track::getInsertIndexForProcessor(const ValueTree &processor, int insertSlot) const {
    const auto &lane = getProcessorLane();
    bool sameLane = lane == Track::getProcessorLaneForProcessor(processor);
    auto handleSameLane = [sameLane](int index) -> int { return sameLane ? std::max(0, index - 1) : index; };
    for (const auto &otherProcessor : lane) {
        if (Processor::getSlot(otherProcessor) >= insertSlot && otherProcessor != processor) {
            int otherIndex = lane.indexOf(otherProcessor);
            return sameLane && lane.indexOf(processor) < otherIndex ? handleSameLane(otherIndex) : otherIndex;
        }
    }
    return handleSameLane(lane.getNumChildren());
}


ValueTree Track::findProcessorNearestToSlot(int slot) const {
    const auto &lane = getProcessorLane();
    auto nearestSlot = INT_MAX;
    ValueTree nearestProcessor;
    for (const auto &processor : lane) {
        int otherSlot = Processor::getSlot(processor);
        if (otherSlot == slot) return processor;

        if (abs(slot - otherSlot) < abs(slot - nearestSlot)) {
            nearestSlot = otherSlot;
            nearestProcessor = processor;
        }
        if (otherSlot > slot) break; // processors are ordered by slot.
    }
    return nearestProcessor;
}

ValueTree Track::findFirstSelectedProcessor() const {
    for (const auto &processor : getProcessorLane())
        if (isSlotSelected(Processor::getSlot(processor)))
            return processor;
    return {};
}

ValueTree Track::findLastSelectedProcessor() const {
    const auto &lane = getProcessorLane();
    for (int i = lane.getNumChildren() - 1; i >= 0; i--) {
        const auto &processor = lane.getChild(i);
        if (isSlotSelected(Processor::getSlot(processor)))
            return processor;
    }
    return {};
}

Array<ValueTree> Track::findSelectedProcessors() const {
    const auto &lane = getProcessorLane();
    Array<ValueTree> selectedProcessors;
    auto selectedSlotsMask = getSlotMask();
    for (const auto &processor : lane)
        if (selectedSlotsMask[Processor::getSlot(processor)])
            selectedProcessors.add(processor);
    return selectedProcessors;
}

Array<ValueTree> Track::getAllProcessors() const {
    Array<ValueTree> allProcessors;
    allProcessors.add(getInputProcessor());
    for (const auto &processor : getProcessorLane()) {
        allProcessors.add(processor);
    }
    allProcessors.add(getOutputProcessor());
    return allProcessors;
}

int Track::getInsertIndexForProcessor(const ValueTree &state, const ValueTree &processor, int insertSlot) {
    const auto &lane = Track::getProcessorLane(state);
    bool sameLane = lane == Track::getProcessorLaneForProcessor(processor);
    auto handleSameLane = [sameLane](int index) -> int { return sameLane ? std::max(0, index - 1) : index; };
    for (const auto &otherProcessor : lane) {
        if (Processor::getSlot(otherProcessor) >= insertSlot && otherProcessor != processor) {
            int otherIndex = lane.indexOf(otherProcessor);
            return sameLane && lane.indexOf(processor) < otherIndex ? handleSameLane(otherIndex) : otherIndex;
        }
    }
    return handleSameLane(lane.getNumChildren());
}

ValueTree Track::findFirstSelectedProcessor(const ValueTree &state) {
    for (const auto &processor : getProcessorLane(state))
        if (isSlotSelected(state, Processor::getSlot(processor)))
            return processor;
    return {};
}

Array<ValueTree> Track::findSelectedProcessors(const ValueTree &state) {
    const auto &lane = getProcessorLane(state);
    Array<ValueTree> selectedProcessors;
    auto selectedSlotsMask = getSlotMask(state);
    for (const auto &processor : lane)
        if (selectedSlotsMask[Processor::getSlot(processor)])
            selectedProcessors.add(processor);
    return selectedProcessors;
}
