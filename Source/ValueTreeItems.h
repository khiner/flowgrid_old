#pragma once

#include <processors/MixerChannelProcessor.h>
#include "view/UiColours.h"
#include "Identifiers.h"

class ValueTreeItem;

class ProcessorLifecycleListener {
public:
    virtual void processorCreated(const ValueTree&) {};
    virtual void processorWillBeDestroyed(const ValueTree &) {};
    virtual void processorHasBeenDestroyed(const ValueTree &) {};
    virtual void processorDragInitiated() {};
    virtual void processorBeingDragged(bool isBackToInitialPosition) {};
    virtual void processorDragAboutToFinalize() {};
    virtual void processorDragFinalized() {};
    virtual ~ProcessorLifecycleListener() = default;
};

// Adapted from JUCE::ChangeBroadcaster to support messaging specific changes to the project.
class ProcessorLifecycleBroadcaster {
public:
    ProcessorLifecycleBroadcaster() = default;

    virtual ~ProcessorLifecycleBroadcaster() = default;

    /*
     * These processor lifecycle methods are here to avoid recursive undoable actions in the ProcessorGraph.
     */

    /** Registers a listener to receive change callbacks from this broadcaster.
        Trying to add a listener that's already on the list will have no effect.
    */
    void addProcessorLifecycleListener(ProcessorLifecycleListener *listener) {
        // Listeners can only be safely added when the event thread is locked
        // You can  use a MessageManagerLock if you need to call this from another thread.
        jassert(MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        listeners.add(listener);
    }

    /** Unregisters a listener from the list.
        If the listener isn't on the list, this won't have any effect.
    */
    void removeProcessorLifecycleListener(ProcessorLifecycleListener *listener) {
        // Listeners can only be safely removed when the event thread is locked
        // You can  use a MessageManagerLock if you need to call this from another thread.
        jassert(MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        listeners.remove(listener);
    }

    virtual void sendProcessorCreatedMessage(const ValueTree& item) {
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            listeners.call(&ProcessorLifecycleListener::processorCreated, item);
        } else {
            MessageManager::callAsync([this, item] {
                listeners.call(&ProcessorLifecycleListener::processorCreated, item);
            });
        }
    }

    // The following methods _cannot_ be called async!
    // They assume ordering of "willBeThinged -> thing -> hasThinged".
    virtual void sendProcessorWillBeDestroyedMessage(const ValueTree& item) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        listeners.call(&ProcessorLifecycleListener::processorWillBeDestroyed, item);
    }

    virtual void sendProcessorHasBeenDestroyedMessage(const ValueTree& item) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        listeners.call(&ProcessorLifecycleListener::processorHasBeenDestroyed, item);
    }

    virtual void sendProcessorDragInitiatedMessage() {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        listeners.call(&ProcessorLifecycleListener::processorDragInitiated);
    }

    virtual void sendProcessorBeingDraggedMessage(bool isBackToInitialPosition) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        listeners.call(&ProcessorLifecycleListener::processorBeingDragged, isBackToInitialPosition);
    }

    virtual void sendProcessorDragAboutToFinalizeMessage() {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        listeners.call(&ProcessorLifecycleListener::processorDragAboutToFinalize);
    }

    virtual void sendProcessorDragFinalizedMessage() {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        listeners.call(&ProcessorLifecycleListener::processorDragFinalized);
    }
private:
    ListenerList <ProcessorLifecycleListener> listeners;

    JUCE_DECLARE_NON_COPYABLE (ProcessorLifecycleBroadcaster)
};
