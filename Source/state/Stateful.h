#pragma once

#include <juce_data_structures/juce_data_structures.h>

using namespace juce;

class Stateful {
public:
    virtual ~Stateful() = default;

    virtual void loadFromState(const ValueTree &state) = 0;

    virtual ValueTree &getState() = 0;

    virtual void clear() {
        getState().removeAllChildren(nullptr);
    }

    void addListener(ValueTree::Listener *listener) {
        getState().addListener(listener);
    }

    void removeListener(ValueTree::Listener *listener) {
        getState().removeListener(listener);
    }

    static void resetVarToInt(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude) {
        tree.setPropertyExcludingListener(listenerToExclude, id, int(tree.getProperty(id)), nullptr);
    };

    static void resetVarToBool(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude) {
        tree.setPropertyExcludingListener(listenerToExclude, id, bool(tree.getProperty(id)), nullptr);
    };
};
