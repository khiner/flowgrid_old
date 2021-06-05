#include "Track.h"

int Track::getInsertIndexForProcessor(Processor *processor, const ProcessorLane *lane, int insertSlot) const {
    const auto *myLane = getProcessorLane();
    bool sameLane = myLane == lane;
    auto handleSameLane = [sameLane](int index) -> int { return sameLane ? std::max(0, index - 1) : index; };
    for (const auto *otherProcessor : myLane->getChildren()) {
        if (otherProcessor->getSlot() >= insertSlot && otherProcessor != processor) {
            int otherIndex = otherProcessor->getIndex();
            return sameLane && myLane->indexOf(processor) < otherIndex ? handleSameLane(otherIndex) : otherIndex;
        }
    }
    return handleSameLane(myLane->size());
}

Processor *Track::findProcessorNearestToSlot(int slot) const {
    auto nearestSlot = INT_MAX;
    Processor *nearestProcessor = nullptr;
    for (auto *processor : getProcessorLane()->getChildren()) {
        int otherSlot = processor->getSlot();
        if (otherSlot == slot) return processor;

        if (abs(slot - otherSlot) < abs(slot - nearestSlot)) {
            nearestSlot = otherSlot;
            nearestProcessor = processor;
        }
        if (otherSlot > slot) break;
    }
    return nearestProcessor;
}

Processor *Track::findFirstSelectedProcessor() const {
    for (auto *processor : getProcessorLane()->getChildren())
        if (isSlotSelected(processor->getSlot()))
            return processor;
    return {};
}

Processor *Track::findLastSelectedProcessor() const {
    const auto *lane = getProcessorLane();
    for (int i = lane->size() - 1; i >= 0; i--) {
        auto *processor = lane->getChild(i);
        if (isSlotSelected(processor->getSlot()))
            return processor;
    }
    return {};
}

Array<Processor *> Track::findSelectedProcessors() const {
    Array<Processor *> selectedProcessors;
    auto selectedSlotsMask = getSlotMask();
    for (auto *processor : getProcessorLane()->getChildren())
        if (selectedSlotsMask[processor->getSlot()])
            selectedProcessors.add(processor);
    return selectedProcessors;
}

Array<Processor *> Track::getAllProcessors() const {
    Array<Processor *> allProcessors;
    allProcessors.add(getInputProcessor());
    for (auto *processor : getProcessorLane()->getChildren()) {
        allProcessors.add(processor);
    }
    allProcessors.add(getOutputProcessor());
    return allProcessors;
}
