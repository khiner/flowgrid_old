#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <utility>

using namespace juce;

template<typename Type>
struct Stateful {
    Stateful() : state(Type::getIdentifier()) {}

    explicit Stateful(ValueTree state): state(std::move(state)) {}

    virtual ~Stateful() = default;

    static bool isType(const ValueTree &state) { return state.hasType(Type::getIdentifier()); }

    // GOAL: remove this method and remove any remaining accesses.
    //  (All state reads/writes should be done through methods.)
    ValueTree &getState() { return state; }

    const ValueTree &getState() const { return state; }

    bool isValid() { return state.isValid(); }

    virtual void loadFromState(const ValueTree &fromState) {
        state.copyPropertiesFrom(fromState, nullptr);
        moveAllChildren(fromState, state, nullptr);
    }

    void loadFromParentState(const ValueTree &parent) {
        loadFromState(parent.getChildWithName(Type::getIdentifier()));
    }

    virtual void clear() { state.removeAllChildren(nullptr); }

    void addListener(ValueTree::Listener *listener) { state.addListener(listener); }
    void removeListener(ValueTree::Listener *listener) { state.removeListener(listener); }

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
