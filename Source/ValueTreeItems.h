#pragma once

#include <processors/SineBank.h>
#include <processors/GainProcessor.h>
#include <processors/MixerChannelProcessor.h>
#include "view/ColourChangeButton.h"
#include "Identifiers.h"

class ValueTreeItem;

class ProjectChangeListener {
public:
    virtual void itemSelected(const ValueTree&) = 0;
    virtual void itemRemoved(const ValueTree&) = 0;
    virtual void processorCreated(const ValueTree&) {};
    virtual void processorWillBeDestroyed(const ValueTree &) {};
    virtual void processorWillBeMoved(const ValueTree &, const ValueTree&, int) {};
    virtual void processorHasMoved(const ValueTree &, const ValueTree&) {};
    virtual ~ProjectChangeListener() {}
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

    // The following three methods _cannot_ be called async!
    // They assume ordering of "willBeThinged -> thing -> hasThinged".
    virtual void sendProcessorWillBeDestroyedMessage(ValueTree item) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        changeListeners.call(&ProjectChangeListener::processorWillBeDestroyed, item);
    }

    virtual void sendProcessorWillBeMovedMessage(const ValueTree& item, const ValueTree& newParent, int insertIndex) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        changeListeners.call(&ProjectChangeListener::processorWillBeMoved, item, newParent, insertIndex);
    }

    virtual void sendProcessorHasMovedMessage(const ValueTree& item, const ValueTree& newParent) {
        jassert(MessageManager::getInstance()->isThisTheMessageThread());
        changeListeners.call(&ProjectChangeListener::processorHasMoved, item, newParent);
    }

private:
    ListenerList <ProjectChangeListener> changeListeners;

    JUCE_DECLARE_NON_COPYABLE (ProjectChangeBroadcaster)
};

/** Creates the various concrete types below. */
ValueTreeItem *createValueTreeItemForType(const ValueTree &, UndoManager &);


class ValueTreeItem : public TreeViewItem,
                      protected ValueTree::Listener {
public:
    ValueTreeItem(ValueTree v, UndoManager &um)
            : state(std::move(v)), undoManager(um) {
        state.addListener(this);
        if (state[IDs::selected]) {
            setSelected(true, false, dontSendNotification);
        }
    }

    ValueTree getState() const {
        return state;
    }

    UndoManager *getUndoManager() const {
        return &undoManager;
    }

    virtual String getDisplayText() {
        return state[IDs::name].toString();
    }

    virtual bool isItemDeletable() {
        return true;
    }

    String getUniqueName() const override {
        if (state.hasProperty(IDs::uuid))
            return state[IDs::uuid].toString();

        return state[IDs::mediaId].toString();
    }

    bool mightContainSubItems() override {
        return state.getNumChildren() > 0;
    }

    void paintItem(Graphics &g, int width, int height) override {
        if (isSelected()) {
            g.setColour(Colours::red);
            g.drawRect({(float) width, (float) height}, 1.5f);
        }

        const auto col = Colour::fromString(state[IDs::colour].toString());

        if (!col.isTransparent())
            g.fillAll(col.withAlpha(0.5f));

        g.setColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::defaultText,
                                           Colours::black));
        g.setFont(15.0f);
        g.drawText(getDisplayText(), 4, 0, width - 4, height,
                   Justification::centredLeft, true);
    }

    void itemOpennessChanged(bool isNowOpen) override {
        if (isNowOpen && getNumSubItems() == 0)
            refreshSubItems();
        else
            clearSubItems();
    }

    void itemSelectionChanged(bool isNowSelected) override {
        state.setProperty(IDs::selected, isNowSelected, nullptr);
    }

    var getDragSourceDescription() override {
        return state.getType().toString();
    }

protected:
    ValueTree state;
    UndoManager &undoManager;

    void valueTreePropertyChanged(ValueTree & tree, const Identifier &identifier) override {
        if (tree != state || tree.getType() == IDs::PARAM)
            return;

        repaintItem();
        if (identifier == IDs::selected && tree[IDs::selected]) {
            if (auto *ov = getOwnerView()) {
                if (auto *cb = dynamic_cast<ProjectChangeBroadcaster *> (ov->getRootItem())) {
                    setSelected(true, false, dontSendNotification); // make sure TreeView UI is up-to-date
                    cb->sendItemSelectedMessage(state);
                }
            }
        }
    }

    void valueTreeChildAdded(ValueTree &parentTree, ValueTree &child) override {
        treeChildrenChanged(parentTree);
        if (parentTree == state) {
            TreeViewItem *childItem = this->getSubItem(parentTree.indexOf(child));
            if (childItem != nullptr) {
                childItem->setSelected(true, true, sendNotification);
                if (child[IDs::selected]) {
                    child.sendPropertyChangeMessage(IDs::selected);
                }
            }
        }
    }

    void valueTreeChildRemoved(ValueTree &parentTree, ValueTree &child, int indexFromWhichChildWasRemoved) override {
        if (parentTree != state)
            return;

        if (auto *ov = getOwnerView()) {
            if (auto *cb = dynamic_cast<ProjectChangeBroadcaster *> (ov->getRootItem())) {
                cb->sendItemRemovedMessage(child);
            }
        }
        treeChildrenChanged(parentTree);

        if (getOwnerView()->getNumSelectedItems() == 0) {
            ValueTree itemToSelect;
            if (parentTree.getNumChildren() == 0)
                itemToSelect = parentTree;
            else if (indexFromWhichChildWasRemoved - 1 >= 0)
                itemToSelect = parentTree.getChild(indexFromWhichChildWasRemoved - 1);
            else
                itemToSelect = parentTree.getChild(indexFromWhichChildWasRemoved);

            if (itemToSelect.isValid()) {
                itemToSelect.setProperty(IDs::selected, true, nullptr);
            }
        }
    }

    void valueTreeChildOrderChanged(ValueTree &parentTree, int, int) override {
        treeChildrenChanged(parentTree);
    }

    void valueTreeParentChanged(ValueTree &) override {}

    void treeChildrenChanged(const ValueTree &parentTree) {
        if (parentTree == state) {
            refreshSubItems();
            treeHasChanged();
            setOpen(true);
        }
    }

private:
    void refreshSubItems() {
        clearSubItems();

        for (const auto &v : state)
            if (auto *item = createValueTreeItemForType(v, undoManager))
                addSubItem(item);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValueTreeItem)
};

namespace Helpers {
    template<typename TreeViewItemType>
    inline OwnedArray<ValueTree> getSelectedTreeViewItems(TreeView &treeView) {
        OwnedArray<ValueTree> items;
        const int numSelected = treeView.getNumSelectedItems();

        for (int i = 0; i < numSelected; ++i)
            if (auto *vti = dynamic_cast<TreeViewItemType *> (treeView.getSelectedItem(i)))
                items.add(new ValueTree(vti->getState()));

        return items;
    }

    template<typename TreeViewItemType>
    inline OwnedArray<ValueTree> getSelectedAndDeletableTreeViewItems(TreeView &treeView) {
        OwnedArray<ValueTree> items;
        const int numSelected = treeView.getNumSelectedItems();

        for (int i = 0; i < numSelected; ++i)
            if (auto *vti = dynamic_cast<TreeViewItemType *> (treeView.getSelectedItem(i))) {
                if (vti->isItemDeletable()) {
                    items.add(new ValueTree(vti->getState()));
                }
            }

        return items;
    }

    inline void moveSingleItem(ValueTree &item, ValueTree newParent, int insertIndex, UndoManager *undoManager) {
        if (item.getParent().isValid() && newParent != item && !newParent.isAChildOf(item)) {
            if (item.getParent() == newParent) {
                if (newParent.indexOf(item) < insertIndex) {
                    --insertIndex;
                }
                item.getParent().moveChild(item.getParent().indexOf(item), insertIndex, undoManager);
            } else {
                item.getParent().removeChild(item, undoManager);
                newParent.addChild(item, insertIndex, undoManager);
            }
        }
    }

    inline void moveItems(TreeView &treeView, const OwnedArray<ValueTree> &items,
                          ValueTree newParent, int insertIndex, UndoManager *undoManager) {
        if (items.isEmpty())
            return;

        std::unique_ptr<XmlElement> oldOpenness(treeView.getOpennessState(false));

        for (int i = items.size(); --i >= 0;) {
            ValueTree &item = *items.getUnchecked(i);
            if (item.hasType(IDs::PROCESSOR)) {
                auto *cb = dynamic_cast<ProjectChangeBroadcaster *> (treeView.getRootItem());
                cb->sendProcessorWillBeMovedMessage(item, newParent, insertIndex);
                moveSingleItem(item, newParent, insertIndex, undoManager);
                cb->sendProcessorHasMovedMessage(item, newParent);
            } else {
                moveSingleItem(item, newParent, insertIndex, undoManager);
            }
        }

        if (oldOpenness != nullptr)
            treeView.restoreOpennessState(*oldOpenness, false);
    }

    inline ValueTree createUuidProperty(ValueTree &v) {
        if (!v.hasProperty(IDs::uuid))
            v.setProperty(IDs::uuid, Uuid().toString(), nullptr);

        return v;
    }
}

class Clip : public ValueTreeItem {
public:
    Clip(const ValueTree &v, UndoManager &um)
            : ValueTreeItem(v, um) {
        jassert(state.hasType(IDs::CLIP));
    }

    bool mightContainSubItems() override {
        return false;
    }

    String getDisplayText() override {
        auto timeRange = Range<double>::withStartAndLength(state[IDs::start], state[IDs::length]);
        return ValueTreeItem::getDisplayText() + " (" + String(timeRange.getStart(), 2) + " - " +
               String(timeRange.getEnd(), 2) + ")";
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &) override {
        return false;
    }
};


class Processor : public ValueTreeItem {
public:
    Processor(const ValueTree &v, UndoManager &um)
            : ValueTreeItem(v, um) {
        jassert(state.hasType(IDs::PROCESSOR));
    }

    bool mightContainSubItems() override {
        return false;
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &) override {
        return false;
    }

    var getDragSourceDescription() override {
        if (state[IDs::name].toString() == MixerChannelProcessor::name())
            return MixerChannelProcessor::name();
        else
            return ValueTreeItem::getDragSourceDescription();
    }
};

class Track : public ValueTreeItem {
public:
    Track(const ValueTree &state, UndoManager &um)
            : ValueTreeItem(state, um) {
        jassert(state.hasType(IDs::TRACK));
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return dragSourceDetails.description == IDs::PROCESSOR.toString();
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &dragSourceDetails, int insertIndex) override {
        if (dragSourceDetails.description == IDs::PROCESSOR.toString()) {
            if (getNumSubItems() < insertIndex || getSubItem(insertIndex - 1)->getDragSourceDescription() == IDs::PROCESSOR.toString()) {
                Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Processor>(*getOwnerView()),
                                   state,
                                   insertIndex, &undoManager);
            }
        }
    }
};

class MasterTrack : public ValueTreeItem {
public:
    MasterTrack(const ValueTree &state, UndoManager &um)
            : ValueTreeItem(state, um) {
        jassert(state.hasType(IDs::MASTER_TRACK));
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return dragSourceDetails.description == IDs::PROCESSOR.toString();
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &, int insertIndex) override {
        Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Processor>(*getOwnerView()), state,
                           insertIndex, &undoManager);
    }

    bool isItemDeletable() override {
        return false;
    }
};

class Tracks : public ValueTreeItem {
public:
    Tracks(const ValueTree &state, UndoManager &um)
            : ValueTreeItem(state, um) {
        jassert(state.hasType(IDs::TRACKS));
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return dragSourceDetails.description == IDs::TRACK.toString();
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &, int insertIndex) override {
        Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Track>(*getOwnerView()), state,
                           insertIndex, &undoManager);
    }

    bool canBeSelected() const override {
        return false;
    }
};

class Project : public ValueTreeItem,
                public ProjectChangeBroadcaster {
public:
    Project(const ValueTree &v, UndoManager &um)
            : ValueTreeItem(v, um) {
        if (!state.isValid()) {
            state = createDefaultProject();
        } else {
            connections = state.getChildWithName(IDs::CONNECTIONS);
            tracks = state.getChildWithName(IDs::TRACKS);
            masterTrack = tracks.getChildWithName(IDs::MASTER_TRACK);
        }
        jassert (state.hasType(IDs::PROJECT));
    }

    ValueTree& getConnections() {
        return connections;
    }

    Array<ValueTree> getConnectionsForNode(AudioProcessorGraph::NodeID nodeId) {
        Array<ValueTree> nodeConnections;
        for (const auto& connection : connections) {
            if (connection.getChildWithProperty(IDs::NODE_ID, int(nodeId)).isValid()) {
                nodeConnections.add(connection);
            }
        }

        return nodeConnections;
    }

    ValueTree& getTracks() {
        return tracks;
    }

    int getNumTracks() {
        return tracks.getNumChildren();
    }

    ValueTree getTrack(int trackIndex) {
        return tracks.getChild(trackIndex);
    }

    ValueTree& getMasterTrack() {
        return masterTrack;
    }

    ValueTree& getSelectedTrack() {
        return selectedTrack;
    }

    ValueTree& getSelectedProcessor() {
        return selectedProcessor;
    }

    void setSelectedProcessor(ValueTree& processor) {
        getOwnerView()->clearSelectedItems();
        processor.setProperty(IDs::selected, true, nullptr);
    }

    bool hasConnections() {
        return connections.isValid() && connections.getNumChildren() > 0;
    }

    const ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) {
        for (auto connectionState : connections) {
            auto sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto destState = connectionState.getChildWithName(IDs::DESTINATION);
            if (AudioProcessorGraph::NodeID(int(sourceState[IDs::NODE_ID])) == connection.source.nodeID &&
                AudioProcessorGraph::NodeID(int(destState[IDs::NODE_ID])) == connection.destination.nodeID &&
                int(sourceState[IDs::CHANNEL]) == connection.source.channelIndex &&
                int(destState[IDs::CHANNEL]) == connection.destination.channelIndex) {
                return connectionState;
            }
        }
        return {};

    }

    void addConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager) {
        if (getConnectionMatching(connection).isValid())
            return; // dupe-add attempt

        ValueTree connectionState(IDs::CONNECTION);
        ValueTree source(IDs::SOURCE);
        source.setProperty(IDs::NODE_ID, int(connection.source.nodeID), nullptr);
        source.setProperty(IDs::CHANNEL, connection.source.channelIndex, nullptr);
        connectionState.addChild(source, -1, nullptr);

        ValueTree destination(IDs::DESTINATION);
        destination.setProperty(IDs::NODE_ID, int(connection.destination.nodeID), nullptr);
        destination.setProperty(IDs::CHANNEL, connection.destination.channelIndex, nullptr);
        connectionState.addChild(destination, -1, nullptr);

        connections.addChild(connectionState, -1, undoManager);
    }

    bool removeConnection(const ValueTree& connection, UndoManager* undoManager) {
        if (connection.isValid()) {
            connections.removeChild(connection, undoManager);
            return true;
        }
        return false;
    }

    bool removeConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager) {
        const ValueTree &connectionState = getConnectionMatching(connection);
        return removeConnection(connectionState, undoManager);
    }

    // make a snapshot of all the information needed to capture AudioGraph connections and UI positions
    void makeConnectionsSnapshot() {
        connectionsSnapshot.clear();
        for (auto connection : connections) {
            connectionsSnapshot.add(connection.createCopy());
        }
        slotForNodeIdSnapshot.clear();
        for (auto track : tracks) {
            for (auto child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    slotForNodeIdSnapshot.insert(std::pair<int, int>(child[IDs::NODE_ID], child[IDs::PROCESSOR_SLOT]));
                }
            }
        }
    }

    void restoreConnectionsSnapshot() {
        connections.removeAllChildren(nullptr);
        for (const auto& connection : connectionsSnapshot) {
            connections.addChild(connection, -1, nullptr);
        }
        for (const auto& track : tracks) {
            for (auto child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    child.setProperty(IDs::PROCESSOR_SLOT, slotForNodeIdSnapshot.at(int(child[IDs::NODE_ID])), nullptr);
                }
            }
        }
    }

    void moveSelectionUp() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::upKey));
    }

    void moveSelectionDown() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::downKey));
    }

    void moveSelectionLeft() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::leftKey));
    }

    void moveSelectionRight() {
        getOwnerView()->keyPressed(KeyPress(KeyPress::rightKey));
    }

    void deleteSelectedItems() {
        auto selectedItems(Helpers::getSelectedAndDeletableTreeViewItems<ValueTreeItem>(*getOwnerView()));

        for (int i = selectedItems.size(); --i >= 0;) {
            ValueTree &v = *selectedItems.getUnchecked(i);

            if (v.getParent().isValid()) {
                if (v.hasType(IDs::PROCESSOR)) {
                    sendProcessorWillBeDestroyedMessage(v);
                }
                v.getParent().removeChild(v, &undoManager);
            }
        }
    }

    ValueTree createDefaultProject() {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        Helpers::createUuidProperty(state);

        tracks = ValueTree(IDs::TRACKS);
        Helpers::createUuidProperty(tracks);
        tracks.setProperty(IDs::name, "Tracks", nullptr);

        masterTrack = ValueTree(IDs::MASTER_TRACK);
        Helpers::createUuidProperty(masterTrack);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        ValueTree masterMixer = createAndAddProcessor(masterTrack, MixerChannelProcessor::name(), false);
        masterMixer.setProperty(IDs::selected, true, nullptr);
        tracks.addChild(masterTrack, -1, nullptr);

        for (int tn = 0; tn < 1; ++tn) {
            ValueTree track = createAndAddTrack(false);
            createAndAddProcessor(track, SineBank::name(), false);

//            for (int cn = 0; cn < 3; ++cn) {
//                ValueTree c(IDs::CLIP);
//                Helpers::createUuidProperty(c);
//                c.setProperty(IDs::name, trackName + ", Clip " + String(cn + 1), nullptr);
//                c.setProperty(IDs::start, cn, nullptr);
//                c.setProperty(IDs::length, 1.0, nullptr);
//                c.setProperty(IDs::selected, false, nullptr);
//
//                track.addChild(c, -1, nullptr);
//            }
        }

        state.addChild(tracks, -1, nullptr);

        connections = ValueTree(IDs::CONNECTIONS);
        state.addChild(connections, -1, nullptr);

        return state;
    }

    ValueTree createAndAddTrack(bool undoable=true) {
        int numTracks = getNumTracks() - 1; // minus 1 because of master track

        ValueTree track(IDs::TRACK);
        const String trackName("Track " + String(numTracks + 1));
        Helpers::createUuidProperty(track);
        track.setProperty(IDs::colour, Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
        track.setProperty(IDs::name, trackName, nullptr);

        const ValueTree &masterTrack = tracks.getChildWithName(IDs::MASTER_TRACK);
        // Insert new processors _before_ the master track, or at the end if there isn't one.
        int insertIndex = masterTrack.isValid() ? tracks.indexOf(masterTrack) : -1;

        tracks.addChild(track, insertIndex, undoable ? &undoManager : nullptr);

        createAndAddProcessor(track, MixerChannelProcessor::name(), undoable);
        return track;
    }

    ValueTree createAndAddProcessor(const String& name, bool undoable=true) {
        ValueTree& selectedTrack = getSelectedTrack();
        if (selectedTrack.isValid()) {
            return createAndAddProcessor(selectedTrack, name, undoable);
        } else {
            return ValueTree();
        }
    }

    ValueTree createAndAddProcessor(ValueTree &track, const String& name, bool undoable=true) {
        ValueTree processor(IDs::PROCESSOR);
        Helpers::createUuidProperty(processor);
        processor.setProperty(IDs::name, name, nullptr);

        // Insert new processors _right before_ the first g&b processor, or at the end if there isn't one.
        int insertIndex = getMaxProcessorInsertIndex(track);
        int slot = 0;
        if (insertIndex == -1 && name == MixerChannelProcessor::name()) {
            slot = NUM_AVAILABLE_PROCESSOR_SLOTS - 1;
        } else if (track.getNumChildren() > 1) {
            slot = int(track.getChild(track.getNumChildren() - 2).getProperty(IDs::PROCESSOR_SLOT)) + 1;
        }

        processor.setProperty(IDs::PROCESSOR_SLOT, slot, nullptr);
        track.addChild(processor, insertIndex, undoable ? &undoManager : nullptr);
        sendProcessorCreatedMessage(processor);

        return processor;
    }

    const ValueTree getMixerChannelProcessorForTrack(const ValueTree& track) {
        return track.getChildWithProperty(IDs::name, MixerChannelProcessor::name());
    }

    int getMaxProcessorInsertIndex(const ValueTree& track) {
        const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
        return mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren() - 1;
    }

    int getMaxAvailableProcessorSlot(const ValueTree& track) {
        const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
        return mixerChannelProcessor.isValid() ? int(mixerChannelProcessor[IDs::PROCESSOR_SLOT]) - 1 : Project::NUM_AVAILABLE_PROCESSOR_SLOTS - 1;
    }

    void makeSlotsValid(const ValueTree& parent, UndoManager *undoManager) {
        std::vector<int> slots;
        for (const ValueTree& child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                slots.push_back(int(child.getProperty(IDs::PROCESSOR_SLOT)));
            }
        }
        std::sort(slots.begin(), slots.end());
        for (int i = 1; i < slots.size(); i++) {
            while (slots[i] <= slots[i - 1]) {
                slots[i] += 1;
            }
        }

        auto iterator = slots.begin();
        for (ValueTree child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                child.setProperty(IDs::PROCESSOR_SLOT, *(iterator++), undoManager);
            }
        }
    }

    int getParentIndexForProcessor(const ValueTree &parent, const ValueTree &processorState, UndoManager* undoManager) {
        auto slot = int(processorState[IDs::PROCESSOR_SLOT]);
        for (ValueTree otherProcessorState : parent) {
            if (processorState == otherProcessorState)
                continue;
            if (otherProcessorState.hasType(IDs::PROCESSOR)) {
                auto otherSlot = int(otherProcessorState[IDs::PROCESSOR_SLOT]);
                if (otherSlot == slot) {
                    if (otherProcessorState.getParent() == processorState.getParent()) {
                        // moving within same parent - need to resolve the "tie" in a way that guarantees the child order changes.
                        int currentIndex = parent.indexOf(processorState);
                        int currentOtherIndex = parent.indexOf(otherProcessorState);
                        if (currentIndex < currentOtherIndex) {
                            otherProcessorState.setProperty(IDs::PROCESSOR_SLOT, otherSlot - 1, undoManager);
                            return currentIndex + 2;
                        } else {
                            otherProcessorState.setProperty(IDs::PROCESSOR_SLOT, otherSlot + 1, undoManager);
                            return currentIndex - 1;
                        }
                    } else {
                        return parent.indexOf(otherProcessorState);
                    }
                } else if (otherSlot > slot) {
                    return parent.indexOf(otherProcessorState);
                }
            }
        }

        // TODO in this and other places, we assume processors are the only type of track child.
        return parent.getNumChildren();
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return false;
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &, int insertIndex) override {}

    bool canBeSelected() const override {
        return false;
    }

    void sendItemSelectedMessage(ValueTree item) override {
        if (item.hasType(IDs::TRACK)) {
            selectedTrack = item;
        } else if (item.getParent().hasType(IDs::TRACK)) {
            selectedTrack = item.getParent();
        } else {
            selectedTrack = ValueTree();
        }
        if (item.hasType(IDs::PROCESSOR)) {
            selectedProcessor = item;
        } else {
            selectedProcessor = ValueTree();
        }

        ProjectChangeBroadcaster::sendItemSelectedMessage(item);
    }

    void sendItemRemovedMessage(ValueTree item) override {
        if (item == selectedTrack) {
            selectedTrack = ValueTree();
        }
        if (item == selectedProcessor) {
            selectedProcessor = ValueTree();
        }
        ProjectChangeBroadcaster::sendItemSelectedMessage(item);
    }

    const static int NUM_VISIBLE_TRACKS = 8;
    const static int NUM_VISIBLE_PROCESSOR_SLOTS = 9;
    // last row is reserved for audio output.
    const static int NUM_AVAILABLE_PROCESSOR_SLOTS = NUM_VISIBLE_PROCESSOR_SLOTS - 1;
private:
    ValueTree tracks;
    ValueTree masterTrack;
    ValueTree selectedTrack;
    ValueTree selectedProcessor;
    ValueTree connections;

    Array<ValueTree> connectionsSnapshot;
    std::unordered_map<int, int> slotForNodeIdSnapshot;
};


inline ValueTreeItem *createValueTreeItemForType(const ValueTree &v, UndoManager &um) {
    if (v.hasType(IDs::PROJECT)) return new Project(v, um);
    if (v.hasType(IDs::TRACKS)) return new Tracks(v, um);
    if (v.hasType(IDs::MASTER_TRACK)) return new MasterTrack(v, um);
    if (v.hasType(IDs::TRACK)) return new Track(v, um);
    if (v.hasType(IDs::PROCESSOR)) return new Processor(v, um);
    if (v.hasType(IDs::CLIP)) return new Clip(v, um);

    return nullptr;
}
