#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

using namespace juce;

// TODO merge with Stateful
template<typename ObjectType, typename CriticalSectionType = DummyCriticalSection>
struct StatefulList : protected ValueTree::Listener {
    explicit StatefulList(ValueTree parentTree) : parent(std::move(parentTree)) {
        parent.addListener(this);
    }

    ~StatefulList() override {
        jassert(size() == 0); // must call freeObjects() in the subclass destructor!
    }

    virtual bool isChildType(const ValueTree &) const = 0;

    int size() const noexcept { return children.size(); }
    int indexOf(ObjectType *object) const { return children.indexOf(object); }

    ObjectType *getChild(int index) const noexcept { return children[index]; }
    const Array<ObjectType *> &getChildren() const { return children; }

    void add(const ValueTree &child, int index) { parent.addChild(child, index, nullptr); }
    void append(const ValueTree &child) { parent.appendChild(child, nullptr); }
    void remove(int index) { parent.removeChild(index, nullptr); }
    void remove(const ValueTree &child) { parent.removeChild(child, nullptr); }

    ObjectType *getChildForState(const ValueTree &state) const {
        if (!state.isValid()) return nullptr;

        for (int i = 0; i < children.size(); ++i) {
            auto *child = children.getUnchecked(i);
            if (child->getState() == state) return child;
        }
        return nullptr;
    }

    int compareElements(ObjectType *first, ObjectType *second) const {
        return parent.indexOf(first->getState()) - parent.indexOf(second->getState());
    }

protected:
    ValueTree parent;

    Array<ObjectType *> children;
    CriticalSectionType arrayLock;
    typedef typename CriticalSectionType::ScopedLockType ScopedLockType;

    virtual ObjectType *createNewObject(const ValueTree &) = 0;
    virtual void deleteObject(ObjectType *) = 0;
    virtual void newObjectAdded(ObjectType *) = 0;
    virtual void objectRemoved(ObjectType *, int oldIndex) = 0;
    virtual void objectOrderChanged() = 0;
    virtual void objectChanged(ObjectType *, const Identifier &i) {}

    // call in the sub-class when being created
    void rebuildObjects() {
        jassert(size() == 0); // must only call this method once at construction
        for (auto child : parent) {
            valueTreeChildAdded(parent, child);
        }
    }

    // call in the sub-class when being destroyed
    void freeObjects() {
        parent.removeListener(this);
        deleteAllObjects();
    }

    void deleteAllObjects() {
        const ScopedLockType sl(arrayLock);
        while (children.size() > 0) {
            {
                // TODO can we just delete the value trees one-by-one
                //  and have this trigger the same behavior in valueTreeChildRemoved?
                const ScopedLockType sl(arrayLock);
                int oldIndex = children.size() - 1;
                ObjectType *o = children.removeAndReturn(oldIndex);
                objectRemoved(o, oldIndex);
                deleteObject(o);
            }
        }
    }

    bool isChildTree(ValueTree &tree) const { return isChildType(tree) && tree.getParent() == parent; }

    int indexOf(const ValueTree &tree) const noexcept {
        for (int i = 0; i < children.size(); ++i)
            if (children.getUnchecked(i)->getState() == tree)
                return i;
        return -1;
    }

    void valueTreeChildAdded(ValueTree &, ValueTree &tree) override {
        if (isChildTree(tree)) {
            const int index = parent.indexOf(tree);
            if (ObjectType *newObject = createNewObject(tree)) {
                {
                    const ScopedLockType sl(arrayLock);
                    if (index == parent.getNumChildren() - 1)
                        children.add(newObject);
                    else
                        children.addSorted(*this, newObject);
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
                    ObjectType *o = children.removeAndReturn(oldIndex);
                    objectRemoved(o, oldIndex);
                    deleteObject(o);
                }
            }
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {
        if (tree == parent) {
            {
                const ScopedLockType sl(arrayLock);
                children.sort(*this);
            }
            objectOrderChanged();
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (isChildTree(tree)) {
            objectChanged(getChildForState(tree), i);
        }
    }
};
