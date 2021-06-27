#pragma once

#include "Processor.h"
#include "StatefulList.h"

namespace ProcessorLaneIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANE)
ID(selectedSlotsMask)
#undef ID
}


struct ProcessorLane : public Stateful<ProcessorLane>, public StatefulList<Processor> {
    ProcessorLane(UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : StatefulList<Processor>(state), undoManager(undoManager), deviceManager(deviceManager) {
        rebuildObjects();
    }

    explicit ProcessorLane(const ValueTree &state, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : Stateful<ProcessorLane>(state), StatefulList<Processor>(state), undoManager(undoManager), deviceManager(deviceManager) {
        rebuildObjects();
    }

    ~ProcessorLane() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return ProcessorLaneIDs::PROCESSOR_LANE; }
    void loadFromState(const ValueTree &fromState) override;
    bool isChildType(const ValueTree &tree) const override { return Processor::isType(tree); }

    int getIndex() const { return state.getParent().indexOf(state); }

    Processor *getProcessorAtSlot(int slot) const {
        for (auto *processor : children)
            if (processor->getSlot() == slot)
                return processor;
        return nullptr;
    }
    Processor *getProcessorByNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        for (auto *processor : children)
            if (processor->getNodeId() == nodeId)
                return processor;
        return nullptr;
    }

    BigInteger getSelectedSlotsMask() const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(state[ProcessorLaneIDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    void setSelectedSlotsMask(const BigInteger &selectedSlotsMask) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr); }

    static void setSelectedSlotsMask(ValueTree &state, const BigInteger &selectedSlotsMask) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr); }

protected:
    Processor *createNewObject(const ValueTree &tree) override { return new Processor(tree, undoManager, deviceManager); }

private:
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;
};
