#pragma once

#include "JuceHeader.h"

namespace Utilities {
    template<typename ObjectType, typename CriticalSectionType = DummyCriticalSection>
    class ValueTreeObjectList : public ValueTree::Listener {
    public:
        explicit ValueTreeObjectList(const ValueTree &parentTree)
                : parent(parentTree) {
            parent.addListener(this);
        }

        virtual ~ValueTreeObjectList() {
            jassert (objects.size() == 0); // must call freeObjects() in the subclass destructor!
        }

        // call in the sub-class when being created
        void rebuildObjects() {
            jassert (objects.size() == 0); // must only call this method once at construction

            for (const auto &v : parent) {
                if (isSuitableType(v)) {
                    if (ObjectType *newObject = createNewObject(v)) {
                        objects.add(newObject);
                    }
                }
            }
        }

        // call in the sub-class when being destroyed
        void freeObjects() {
            parent.removeListener(this);
            deleteAllObjects();
        }


        virtual bool isSuitableType(const ValueTree &) const = 0;

        virtual ObjectType *createNewObject(const ValueTree &) = 0;

        virtual void deleteObject(ObjectType *) = 0;

        virtual void newObjectAdded(ObjectType *) = 0;

        virtual void objectRemoved(ObjectType *) = 0;

        virtual void objectOrderChanged() = 0;


        void valueTreeChildAdded(ValueTree &, ValueTree &tree) override {
            if (isChildTree(tree)) {
                const int index = parent.indexOf(tree);
                (void) index;
                jassert (index >= 0);

                if (ObjectType *newObject = createNewObject(tree)) {
                    {
                        const ScopedLockType sl(arrayLock);

                        if (index == parent.getNumChildren() - 1)
                            objects.add(newObject);
                        else
                            objects.addSorted(*this, newObject);
                    }

                    newObjectAdded(newObject);
                } else
                    jassertfalse;
            }
        }

        void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int) override {
            if (parent == exParent && isSuitableType(tree)) {
                const int oldIndex = indexOf(tree);

                if (oldIndex >= 0) {
                    ObjectType *o;

                    {
                        const ScopedLockType sl(arrayLock);
                        o = objects.removeAndReturn(oldIndex);

                        objectRemoved(o);
                        deleteObject(o);
                    }
                }
            }
        }

        void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {
            if (tree == parent) {
                {
                    const ScopedLockType sl(arrayLock);
                    sortArray();
                }

                objectOrderChanged();
            }
        }

        void valueTreePropertyChanged(ValueTree &, const Identifier &) override {}

        void valueTreeParentChanged(ValueTree &) override {}

        void valueTreeRedirected(ValueTree &) override { jassertfalse; } // may need to add handling if this is hit

        Array<ObjectType *> objects;
        CriticalSectionType arrayLock;
        typedef typename CriticalSectionType::ScopedLockType ScopedLockType;

    protected:
        ValueTree parent;

        void deleteAllObjects() {
            const ScopedLockType sl(arrayLock);

            while (objects.size() > 0)
                deleteObject(objects.removeAndReturn(objects.size() - 1));
        }

        bool isChildTree(ValueTree &v) const {
            return isSuitableType(v) && v.getParent() == parent;
        }

        int indexOf(const ValueTree &v) const noexcept {
            for (int i = 0; i < objects.size(); ++i)
                if (objects.getUnchecked(i)->getState() == v)
                    return i;

            return -1;
        }

        void sortArray() {
            objects.sort(*this);
        }

    public:
        int compareElements(ObjectType *first, ObjectType *second) const {
            int index1 = parent.indexOf(first->getState());
            int index2 = parent.indexOf(second->getState());
            return index1 - index2;
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeObjectList)
    };
}
