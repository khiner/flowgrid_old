#include "Stateful.h"

void Stateful::resetVarToInt(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude) {
    tree.setPropertyExcludingListener(listenerToExclude, id, int(tree.getProperty(id)), nullptr);
}

void Stateful::resetVarToBool(ValueTree &tree, const Identifier &id, ValueTree::Listener *listenerToExclude) {
    tree.setPropertyExcludingListener(listenerToExclude, id, bool(tree.getProperty(id)), nullptr);
}

void Stateful::moveAllChildren(ValueTree fromParent, ValueTree &toParent, UndoManager *undoManager) {
    while (fromParent.getNumChildren() > 0) {
        const auto &child = fromParent.getChild(0);
        fromParent.removeChild(0, undoManager);
        toParent.appendChild(child, undoManager);
    }
}
