#pragma once

#include "JuceHeader.h"

namespace Utilities {

/** Iterates through a list of Components, calling a function on each.
    @param fn   The signature of the fn should be equivalent to "void fn (Component* c)"
*/
    template<typename FunctionType>
    inline void visitComponents(std::initializer_list<Component *> comps, FunctionType &&fn) {
        std::for_each(std::begin(comps), std::end(comps), fn);
    }

/** Adds and makes visible any number of Components to a parent. */
    inline void addAndMakeVisible(Component &parent, Array<Component *> comps) {
        std::for_each(std::begin(comps), std::end(comps),
                      [&parent](Component *c) { parent.addAndMakeVisible(c); });
    }

/** Adds and makes visible any number of Components to a parent. */
    inline void addAndMakeVisible(Component &parent, std::initializer_list<Component *> comps) {
        std::for_each(std::begin(comps), std::end(comps),
                      [&parent](Component *c) { parent.addAndMakeVisible(c); });
    }

/** Attempts to load a ValueTree from a file. */
    static inline ValueTree loadValueTree(const File &file, bool asXml) {
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

/** Saves a ValueTree to a File. */
    static inline bool saveValueTree(const ValueTree &v, const File &file, bool asXml) {
        const TemporaryFile temp(file);

        {
            FileOutputStream os(temp.getFile());

            if (!os.getStatus().wasOk())
                return false;

            if (asXml) {
                if (auto xml = std::unique_ptr<XmlElement>(v.createXml()))
                    xml->writeToStream(os, StringRef());
            } else {
                v.writeToStream(os);
            }
        }

        if (temp.getFile().existsAsFile())
            return temp.overwriteTargetFileWithTemporary();

        return false;
    }

    static inline void moveAllChildren(ValueTree fromParent, ValueTree& toParent, UndoManager* undoManager) {
        while (fromParent.getNumChildren() > 0) {
            const auto& child = fromParent.getChild(0);
            fromParent.removeChild(0, undoManager);
            toParent.addChild(child, -1, undoManager);
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
