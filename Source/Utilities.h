#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

namespace Utilities {

/** Attempts to load a ValueTree from a file. */
    static ValueTree loadValueTree(const File &file, bool asXml) {
        if (asXml) {
            if (auto xml = std::unique_ptr<XmlElement>(XmlDocument::parse(file)))
                return ValueTree::fromXml(*xml);
        } else {
            FileInputStream is(file);
            if (is.openedOk())
                return ValueTree::readFromStream(is);
        }

        return {};
    }

    static void moveAllChildren(ValueTree fromParent, ValueTree &toParent, UndoManager *undoManager) {
        while (fromParent.getNumChildren() > 0) {
            const auto &child = fromParent.getChild(0);
            fromParent.removeChild(0, undoManager);
            toParent.appendChild(child, undoManager);
        }
    }

/**
    Utility wrapper for ValueTree::Listener's that only want to override valueTreePropertyChanged.
*/
    struct ValueTreePropertyChangeListener : public ValueTree::Listener {
        void valueTreeChildAdded(ValueTree &, ValueTree &) override {}

        void valueTreeChildRemoved(ValueTree &, ValueTree &, int) override {}

        void valueTreeChildOrderChanged(ValueTree &, int, int) override {}

        void valueTreeParentChanged(ValueTree &) override {}

        void valueTreeRedirected(ValueTree &) override {}
    };
}
