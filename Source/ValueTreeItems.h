#pragma once

#include <processors/MixerChannelProcessor.h>
#include "view/UiColours.h"
#include "Identifiers.h"

class ValueTreeItem;

class ProjectChangeListener {
public:
    virtual void itemRemoved(const ValueTree&) = 0;
    virtual void processorCreated(const ValueTree&) {};
    virtual void processorWillBeDestroyed(const ValueTree &) {};
    virtual void processorHasBeenDestroyed(const ValueTree &) {};
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
    void addProjectChangeListener(ProjectChangeListener *listener) {
        // Listeners can only be safely added when the event thread is locked
        // You can  use a MessageManagerLock if you need to call this from another thread.
        jassert(MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        changeListeners.add(listener);
    }

    /** Unregisters a listener from the list.
        If the listener isn't on the list, this won't have any effect.
    */
    void removeProjectChangeListener(ProjectChangeListener *listener) {
        // Listeners can only be safely removed when the event thread is locked
        // You can  use a MessageManagerLock if you need to call this from another thread.
        jassert(MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        changeListeners.remove(listener);
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
private:
    ListenerList <ProjectChangeListener> changeListeners;

    JUCE_DECLARE_NON_COPYABLE (ProjectChangeBroadcaster)
};

namespace Helpers {
    inline void moveSingleItem(ValueTree &item, ValueTree newParent, int insertIndex, UndoManager *undoManager) {
        newParent.moveChildFromParent(item.getParent(), item.getParent().indexOf(item), insertIndex, undoManager);
    }
}

