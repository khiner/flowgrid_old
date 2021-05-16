#pragma once

#include <juce_data_structures/juce_data_structures.h>

using namespace juce;

template<typename Type>
class Stateful {
public:
    Stateful() : state(Type::getIdentifier()) {}

    virtual ~Stateful() = default;

    static bool isType(const ValueTree &state) { return state.hasType(Type::getIdentifier()); }

    // TODO remove this method and remove any remaining accesses.
    //  (All state accesses should be done through accessor methods.)
    ValueTree &getState() { return state; }

    virtual void loadFromState(const ValueTree &fromState) {
        state.copyPropertiesFrom(fromState, nullptr);
        moveAllChildren(fromState, state, nullptr);
    }

    void loadFromParentState(const ValueTree &parent) {
        loadFromState(parent.getChildWithName(Type::getIdentifier()));
    }

    virtual void clear() {
        state.removeAllChildren(nullptr);
    }

    void addListener(ValueTree::Listener *listener) {
        state.addListener(listener);
    }

    void removeListener(ValueTree::Listener *listener) {
        state.removeListener(listener);
    }

protected:
    ValueTree state;

    static void resetVarToInt(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude) {
        tree.setPropertyExcludingListener(listenerToExclude, id, int(tree.getProperty(id)), nullptr);
    }

    static void resetVarToBool(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude) {
        tree.setPropertyExcludingListener(listenerToExclude, id, bool(tree.getProperty(id)), nullptr);
    }

    static void moveAllChildren(ValueTree fromParent, ValueTree &toParent, UndoManager *undoManager) {
        while (fromParent.getNumChildren() > 0) {
            const auto &child = fromParent.getChild(0);
            fromParent.removeChild(0, undoManager);
            toParent.appendChild(child, undoManager);
        }
    }
};
