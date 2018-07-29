#pragma once

#include <processors/MixerChannelProcessor.h>
#include "view/UiColours.h"
#include "Identifiers.h"

class ValueTreeItem;

class ProjectChangeListener {
public:
    virtual void itemSelected(const ValueTree&) = 0;
    virtual void itemRemoved(const ValueTree&) = 0;
    virtual void processorCreated(const ValueTree&) {};
    virtual void processorWillBeDestroyed(const ValueTree &) {};
    virtual void processorHasBeenDestroyed(const ValueTree &) {};
    virtual void processorWillBeMoved(const ValueTree &, const ValueTree&) {};
    virtual void processorHasMoved(const ValueTree &, const ValueTree&) {};
    virtual ~ProjectChangeListener() = default;
};

// Adapted from JUCE::ChangeBroadcaster to support messaging specific changes to the project.
class ProjectChangeBroadcaster {
public:
    ProjectChangeBroadcaster() = default;

    virtual ~ProjectChangeBroadcaster() = default;

    /** Registers a listener to receive change callbacks from this broadcaster.
        Trying to add a listener that's already on the list will have no effect.
    */
    void addChangeListener (ProjectChangeListener* listener) {
        // Listeners can only be safely added when the event thread is locked
        // You can  use a MessageManagerLock if you need to call this from another thread.
        jassert (MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        changeListeners.add(listener);
    }

    /** Unregisters a listener from the list.
        If the listener isn't on the list, this won't have any effect.
    */
    void removeChangeListener (ProjectChangeListener* listener) {
        // Listeners can only be safely removed when the event thread is locked
        // You can  use a MessageManagerLock if you need to call this from another thread.
        jassert (MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        changeListeners.remove(listener);
    }

    virtual void sendItemSelectedMessage(ValueTree item) {
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            changeListeners.call(&ProjectChangeListener::itemSelected, item);
        } else {
            MessageManager::callAsync([this, item] {
                changeListeners.call(&ProjectChangeListener::itemSelected, item);
            });
        }
    }

    virtual void sendItemRemovedMessage(ValueTree item) {
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            changeListeners.call(&ProjectChangeListener::itemRemoved, item);
        } else {
            MessageManager::callAsync([this, item] {
                changeListeners.call(&ProjectChangeListener::itemRemoved, item);
            });
        }
    }

    virtual void sendProcessorCreatedMessage(ValueTree item) {
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            changeListeners.call(&ProjectChangeListener::processorCreated, item);
        } else {
            MessageManager::callAsync([this, item] {
                changeListeners.call(&ProjectChangeListener::processorCreated, item);
            });
        }
    }

    // The following four methods _cannot_ be called async!
    // They assume ordering of "willBeThinged -> thing -> hasThinged".
    virtual void sendProcessorWillBeDestroyedMessage(ValueTree item) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        changeListeners.call(&ProjectChangeListener::processorWillBeDestroyed, item);
    }

    virtual void sendProcessorHasBeenDestroyedMessage(ValueTree item) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        changeListeners.call(&ProjectChangeListener::processorHasBeenDestroyed, item);
    }

    virtual void sendProcessorWillBeMovedMessage(const ValueTree& item, const ValueTree& newParent) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        changeListeners.call(&ProjectChangeListener::processorWillBeMoved, item, newParent);
    }

    virtual void sendProcessorHasMovedMessage(const ValueTree& item, const ValueTree& newParent) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        changeListeners.call(&ProjectChangeListener::processorHasMoved, item, newParent);
    }

private:
    ListenerList <ProjectChangeListener> changeListeners;

    JUCE_DECLARE_NON_COPYABLE (ProjectChangeBroadcaster)
};

namespace Helpers {
    inline void moveSingleItem(ValueTree &item, ValueTree newParent, int insertIndex, UndoManager *undoManager) {
        newParent.moveChildFromParent(item.getParent(), item.getParent().indexOf(item), insertIndex, undoManager);
    }

    inline void moveItems(ProjectChangeBroadcaster *cb, const OwnedArray<ValueTree> &items,
                          const ValueTree &newParent, int insertIndex, UndoManager *undoManager) {
        if (items.isEmpty())
            return;

        for (int i = items.size(); --i >= 0;) {
            ValueTree &item = *items.getUnchecked(i);
            if (item.hasType(IDs::PROCESSOR)) {
                cb->sendProcessorWillBeMovedMessage(item, newParent);
                moveSingleItem(item, newParent, insertIndex, undoManager);
                cb->sendProcessorHasMovedMessage(item, newParent);
            } else {
                moveSingleItem(item, newParent, insertIndex, undoManager);
            }
        }
    }

    inline ValueTree createUuidProperty(ValueTree &v) {
        if (!v.hasProperty(IDs::uuid))
            v.setProperty(IDs::uuid, Uuid().toString(), nullptr);

        return v;
    }
}

