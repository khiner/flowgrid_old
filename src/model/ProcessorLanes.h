#pragma once

#include "ProcessorLane.h"

namespace ProcessorLanesIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANES)
#undef ID
}


struct ProcessorLanes : public Stateful<ProcessorLanes>, public StatefulList<ProcessorLane> {
    ProcessorLanes(UndoManager &undoManager, AudioDeviceManager &deviceManager);

    explicit ProcessorLanes(const ValueTree &state, UndoManager &undoManager, AudioDeviceManager &deviceManager);

    ~ProcessorLanes() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return ProcessorLanesIDs::PROCESSOR_LANES; }

    void loadFromState(const ValueTree &fromState) override;
    bool isChildType(const ValueTree &tree) const override { return ProcessorLane::isType(tree); }

protected:
    ProcessorLane *createNewObject(const ValueTree &tree) override { return new ProcessorLane(tree, undoManager, deviceManager); }

private:
    ListenerList<Listener> listeners;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;
};
