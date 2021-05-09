#pragma once

template<typename ObjectType, typename CriticalSectionType = DummyCriticalSection>
class ValueTreeObjectList : public ValueTree::Listener {
public:
    explicit ValueTreeObjectList(ValueTree parentTree)
            : parent(std::move(parentTree)) {
        parent.addListener(this);
    }

    ~ValueTreeObjectList() override {
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


    virtual bool isSuitableType(const ValueTree &) const = 0;

    virtual ObjectType *createNewObject(const ValueTree &) = 0;

    virtual void deleteObject(ObjectType *) = 0;

    virtual void newObjectAdded(ObjectType *) = 0;

    virtual void objectRemoved(ObjectType *) = 0;

    virtual void objectOrderChanged() = 0;

    void valueTreeChildAdded(ValueTree &, ValueTree &tree) override {
        if (isChildTree(tree)) {
            const int index = parent.indexOf(tree);
            jassert(index >= 0);

            if (ObjectType *newObject = createNewObject(tree)) {
                {
                    const ScopedLockType sl(arrayLock);
                    if (index == parent.getNumChildren() - 1)
                        objects.add(newObject);
                    else
                        objects.addSorted(*this, newObject);
                }

                newObjectAdded(newObject);
            } else {
                jassertfalse;
            }
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
                objects.sort(*this);
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

    bool isChildTree(ValueTree &tree) const {
        return isSuitableType(tree) && tree.getParent() == parent;
    }

    int indexOf(const ValueTree &tree) const noexcept {
        for (int i = 0; i < objects.size(); ++i)
            if (objects.getUnchecked(i)->getState() == tree)
                return i;

        return -1;
    }

public:
    int compareElements(ObjectType *first, ObjectType *second) const {
        int index1 = parent.indexOf(first->getState());
        int index2 = parent.indexOf(second->getState());
        return index1 - index2;
    }
};
