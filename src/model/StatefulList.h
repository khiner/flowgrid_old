#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

using namespace juce;

// TODO merge with Stateful
template<typename ObjectType, typename CriticalSectionType = DummyCriticalSection>
class StatefulList : public ValueTree::Listener {
public:
    explicit StatefulList(ValueTree parentTree) : parent(std::move(parentTree)) {
        parent.addListener(this);
    }

    ~StatefulList() override {
        jassert(objects.size() == 0); // must call freeObjects() in the subclass destructor!
    }

    // call in the sub-class when being created
    void rebuildObjects() {
        jassert(objects.size() == 0); // must only call this method once at construction
        for (auto child : parent) {
            valueTreeChildAdded(parent, child);
        }
    }

    // call in the sub-class when being destroyed
    void freeObjects() {
        parent.removeListener(this);
        deleteAllObjects();
    }

    virtual bool isChildType(const ValueTree &) const = 0;

    virtual ObjectType *createNewObject(const ValueTree &) = 0;
    virtual void deleteObject(ObjectType *) = 0;
    virtual void newObjectAdded(ObjectType *) = 0;
    virtual void objectRemoved(ObjectType *) = 0;
    virtual void objectOrderChanged() = 0;

    void valueTreeChildAdded(ValueTree &, ValueTree &tree) override {
        if (isChildTree(tree)) {
            const int index = parent.indexOf(tree);
            if (ObjectType *newObject = createNewObject(tree)) {
                {
                    const ScopedLockType sl(arrayLock);
                    if (index == parent.getNumChildren() - 1)
                        objects.add(newObject);
                    else
                        objects.addSorted(*this, newObject);
                }

                newObjectAdded(newObject);
            }
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int) override {
        if (parent == exParent && isChildType(tree)) {
            const int oldIndex = indexOf(tree);
            if (oldIndex >= 0) {
                {
                    const ScopedLockType sl(arrayLock);
                    ObjectType *o = objects.removeAndReturn(oldIndex);
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
                objects.sort(*this);
            }
            objectOrderChanged();
        }
    }

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

    bool isChildTree(ValueTree &tree) const { return isChildType(tree) && tree.getParent() == parent; }

    int indexOf(const ValueTree &tree) const noexcept {
        for (int i = 0; i < objects.size(); ++i)
            if (objects.getUnchecked(i)->getState() == tree)
                return i;
        return -1;
    }

public:
    int compareElements(ObjectType *first, ObjectType *second) const {
        return parent.indexOf(first->getState()) - parent.indexOf(second->getState());
    }
};
