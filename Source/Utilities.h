#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

namespace Utilities {
    static void moveAllChildren(ValueTree fromParent, ValueTree &toParent, UndoManager *undoManager) {
        while (fromParent.getNumChildren() > 0) {
            const auto &child = fromParent.getChild(0);
            fromParent.removeChild(0, undoManager);
            toParent.appendChild(child, undoManager);
        }
    }
}
