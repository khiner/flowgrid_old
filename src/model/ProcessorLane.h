#pragma once

#include "Stateful.h"
#include "Processor.h"
#include "StatefulList.h"

namespace ProcessorLaneIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANE)
ID(selectedSlotsMask)
#undef ID
}


struct ProcessorLane : public Stateful<ProcessorLane>, public StatefulList<Processor> {
    struct Listener {
        virtual void processorAdded(Processor *processor) = 0;
        virtual void processorRemoved(Processor *processor, int oldIndex) = 0;
        virtual void processorOrderChanged() {}
        virtual void processorPropertyChanged(Processor *processor, const Identifier &i) {}
    };

    void addProcessorLaneListener(Listener *listener) {
        listeners.add(listener);
    }
    void removeProcessorLaneListener(Listener *listener) { listeners.remove(listener); }

    ProcessorLane();
    explicit ProcessorLane(const ValueTree &state): Stateful<ProcessorLane>(state), StatefulList<Processor>(state) {
        rebuildObjects();
    }

    ~ProcessorLane() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return ProcessorLaneIDs::PROCESSOR_LANE; }
    void loadFromState(const ValueTree &fromState) override;
    bool isChildType(const ValueTree &tree) const override { return Processor::isType(tree); }

    int getIndex() const { return state.getParent().indexOf(state); }

    BigInteger getSelectedSlotsMask() const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(state[ProcessorLaneIDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    void setSelectedSlotsMask(const BigInteger &selectedSlotsMask) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr); }

    static void setSelectedSlotsMask(ValueTree &state, const BigInteger &selectedSlotsMask) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr); }

protected:
    Processor *createNewObject(const ValueTree &tree) override { return new Processor(tree); }
    void deleteObject(Processor *processor) override { delete processor; }
    void newObjectAdded(Processor *processor) override { listeners.call([processor](Listener &l) { l.processorAdded(processor); }); }
    void objectRemoved(Processor *processor, int oldIndex) override { listeners.call([processor, oldIndex](Listener &l) { l.processorRemoved(processor, oldIndex); }); }
    void objectOrderChanged() override { listeners.call([](Listener &l) { l.processorOrderChanged(); }); }
    void objectChanged(Processor *processor, const Identifier &i) override { listeners.call([processor, i](Listener &l) { l.processorPropertyChanged(processor, i); }); }

private:
    ListenerList<Listener> listeners;
};
