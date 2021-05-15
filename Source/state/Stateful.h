#pragma once

#include <juce_data_structures/juce_data_structures.h>

using namespace juce;

class Stateful {
public:
    virtual ~Stateful() = default;

    // TODO remove this method and remove any remaining accesses.
    //  (All state accesses should be done through accessor methods.)
    virtual ValueTree &getState() = 0;

    virtual void loadFromState(const ValueTree &fromState) = 0;

    virtual void loadFromParentState(const ValueTree &parent) = 0;

    virtual void clear() {
        getState().removeAllChildren(nullptr);
    }

    void addListener(ValueTree::Listener *listener) {
        getState().addListener(listener);
    }

    void removeListener(ValueTree::Listener *listener) {
        getState().removeListener(listener);
    }

protected:
    static void resetVarToInt(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude);

    static void resetVarToBool(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude);

    static void moveAllChildren(ValueTree fromParent, ValueTree &toParent, UndoManager *undoManager);
};
