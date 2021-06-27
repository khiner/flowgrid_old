#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Stateful.h"

using namespace juce;

// TODO merge with Stateful
template<typename ObjectType>
struct StatefulList : protected ValueTree::Listener {
    struct Listener {
        virtual void onChildAdded(ObjectType *child) {}
        virtual void onChildRemoved(ObjectType *child, int oldIndex) {}
        virtual void onOrderChanged() {}
        virtual void onChildChanged(ObjectType *child, const Identifier &i) {}
    };

    virtual bool isChildType(const ValueTree &tree) const = 0;

protected:
    virtual ObjectType *createNewObject(const ValueTree &) = 0;
    virtual void onChildAdded(ObjectType *) {}
    virtual void onChildRemoved(ObjectType *, int oldIndex) {}
    virtual void onOrderChanged() {}
    virtual void onChildChanged(ObjectType *, const Identifier &i) {}

public:
    explicit StatefulList(ValueTree parentTree) : parent(std::move(parentTree)) {
        parent.addListener(this);
    }
    ~StatefulList() override {
        jassert(size() == 0); // must call freeObjects() in the subclass destructor!
    }

    void addListener(Listener *listener) {
        if (listener == nullptr) return;

        listeners.add(listener);
        for (auto *lane : getChildren()) {
            listener->onChildAdded(lane);
        }
    }
    void removeListener(Listener *listener) {
        if (listener == nullptr) return;

        listeners.remove(listener);
    }

    void addChildListener(Listener *listener) { addListener(listener); }
    void removeChildListener(Listener *listener) { removeListener(listener); }

    int size() const noexcept { return children.size(); }
    int indexOf(ObjectType *object) const { return children.indexOf(object); }

    ObjectType *get(int index) const noexcept { return children[index]; }
    const Array<ObjectType *> &getChildren() const { return children; }
    ObjectType *getMostRecentlyCreated() const { return mostRecentlyCreatedObject; }

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
    ListenerList<Listener> listeners;
    ObjectType *mostRecentlyCreatedObject{};

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
        while (children.size() > 0) {
            // TODO can we just delete the value trees one-by-one
            //  and have this trigger the same behavior in valueTreeChildRemoved?
            int oldIndex = children.size() - 1;
            ObjectType *o = children.removeAndReturn(oldIndex);
            onChildRemoved(o, oldIndex);
            deleteChild(o);
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
                if (index == parent.getNumChildren() - 1)
                    children.add(newObject);
                else
                    children.addSorted(*this, newObject);
                mostRecentlyCreatedObject = newObject;
                onChildAdded(newObject);
                listeners.call(&Listener::onChildAdded, newObject);
            }
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int) override {
        if (parent == exParent && isChildType(tree)) {
            const int oldIndex = indexOf(tree);
            if (oldIndex >= 0) {
                auto *child = children.removeAndReturn(oldIndex);
                listeners.call(&Listener::onChildRemoved, child, oldIndex);
                // Not correct but doesn't leave dangling pointers
                // TODO queue
                if (child == mostRecentlyCreatedObject) mostRecentlyCreatedObject = nullptr;
                onChildRemoved(child, oldIndex);
                deleteChild(child);
            }
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {
        if (tree == parent) {
            children.sort(*this);
            onOrderChanged();
            listeners.call(&Listener::onOrderChanged);
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (isChildTree(tree)) {
            auto *child = getChildForState(tree);
            onChildChanged(child, i);
            listeners.call(&Listener::onChildChanged, child, i);
        }
    }

private:
    void deleteChild(ObjectType *child) { delete child; }
};
