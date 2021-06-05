#pragma once

#include "Stateful.h"
#include "ProcessorLane.h"

namespace ProcessorLanesIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANES)
#undef ID
}


struct ProcessorLanes : public Stateful<ProcessorLanes>, public StatefulList<ProcessorLane> {
    struct Listener {
        virtual void processorLaneAdded(ProcessorLane *processorLane) = 0;
        virtual void processorLaneRemoved(ProcessorLane *processorLane, int oldIndex) = 0;
        virtual void processorLaneOrderChanged() {}
        virtual void processorLanePropertyChanged(ProcessorLane *processorLane, const Identifier &i) {}
    };

    void addProcessorLanesListener(Listener *listener) { listeners.add(listener); }
    void removeProcessorLanesListener(Listener *listener) { listeners.remove(listener); }

    ProcessorLanes();

    explicit ProcessorLanes(const ValueTree &state);

    ~ProcessorLanes() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return ProcessorLanesIDs::PROCESSOR_LANES; }

    void loadFromState(const ValueTree &fromState) override;
    bool isChildType(const ValueTree &tree) const override { return ProcessorLane::isType(tree); }

protected:
    ProcessorLane *createNewObject(const ValueTree &tree) override { return new ProcessorLane(tree); }
    void deleteObject(ProcessorLane *processorLane) override { delete processorLane; }
    void newObjectAdded(ProcessorLane *processorLane) override {
        listeners.call([processorLane](Listener &l) { l.processorLaneAdded(processorLane); });
    }
    void objectRemoved(ProcessorLane *processorLane, int oldIndex) override {
        listeners.call([processorLane, oldIndex](Listener &l) { l.processorLaneRemoved(processorLane, oldIndex); });
    }
    void objectOrderChanged() override { listeners.call([](Listener &l) { l.processorLaneOrderChanged(); }); }
    void objectChanged(ProcessorLane *lane, const Identifier &i) override { listeners.call([lane, i](Listener &l) { l.processorLanePropertyChanged(lane, i); }); }

private:
    ListenerList<Listener> listeners;
};
