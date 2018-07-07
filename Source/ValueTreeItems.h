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

    inline void moveSingleItem(ValueTree &item, ValueTree newParent, int insertIndex, UndoManager &undoManager) {
        if (item.getParent().isValid() && newParent != item && !newParent.isAChildOf(item)) {
            if (item.getParent() == newParent) {
                if (newParent.indexOf(item) < insertIndex) {
                    --insertIndex;
                }
                item.getParent().moveChild(item.getParent().indexOf(item), insertIndex, &undoManager);
            } else {
                item.getParent().removeChild(item, &undoManager);
                newParent.addChild(item, insertIndex, &undoManager);
            }
        }
    }

    inline void moveItems(TreeView &treeView, const OwnedArray<ValueTree> &items,
                          ValueTree newParent, int insertIndex, UndoManager &undoManager) {
        if (items.isEmpty())
            return;

        std::unique_ptr<XmlElement> oldOpenness(treeView.getOpennessState(false));

        for (int i = items.size(); --i >= 0;) {
            moveSingleItem(*items.getUnchecked(i), newParent, insertIndex, undoManager);
        }

        if (oldOpenness != nullptr)
            treeView.restoreOpennessState(*oldOpenness, false);
    }

    inline ValueTree createUuidProperty(ValueTree &v) {
        if (!v.hasProperty(IDs::uuid))
            v.setProperty(IDs::uuid, Uuid().toString(), nullptr);

        return v;
    }

    inline void deleteSelectedItems(TreeView &treeView, UndoManager &undoManager) {
        auto selectedItems(Helpers::getSelectedAndDeletableTreeViewItems<ValueTreeItem>(treeView));

        for (int i = selectedItems.size(); --i >= 0;) {
            ValueTree &v = *selectedItems.getUnchecked(i);

            if (v.getParent().isValid())
                v.getParent().removeChild(v, &undoManager);
        }
    }
}

class Clip : public ValueTreeItem {
public:
    Clip(const ValueTree &v, UndoManager &um)
            : ValueTreeItem(v, um) {
        jassert (state.hasType(IDs::CLIP));
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
        jassert (state.hasType(IDs::PROCESSOR));
    }

    bool mightContainSubItems() override {
        return false;
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &) override {
        return false;
    }
};

class Track : public ValueTreeItem {
public:
    Track(const ValueTree &state, UndoManager &um)
            : ValueTreeItem(state, um) {
        jassert (state.hasType(IDs::TRACK));
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return dragSourceDetails.description == IDs::CLIP.toString() ||
               dragSourceDetails.description == IDs::PROCESSOR.toString();
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &dragSourceDetails, int insertIndex) override {
        if (dragSourceDetails.description == IDs::PROCESSOR.toString()) {
            Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Processor>(*getOwnerView()), state,
                               insertIndex, undoManager);
        } else if (dragSourceDetails.description == IDs::CLIP.toString()) {
            Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Clip>(*getOwnerView()), state,
                               insertIndex, undoManager);
        }
    }
};

class MasterTrack : public ValueTreeItem {
public:
    MasterTrack(const ValueTree &state, UndoManager &um)
            : ValueTreeItem(state, um) {
        jassert (state.hasType(IDs::MASTER_TRACK));
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return dragSourceDetails.description == IDs::PROCESSOR.toString();
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &, int insertIndex) override {
        Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Processor>(*getOwnerView()), state,
                           insertIndex, undoManager);
    }

    bool isItemDeletable() override {
        return false;
    }
};

class Tracks : public ValueTreeItem {
public:
    Tracks(const ValueTree &state, UndoManager &um)
            : ValueTreeItem(state, um) {
        jassert (state.hasType(IDs::TRACKS));
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return dragSourceDetails.description == IDs::TRACK.toString();
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &, int insertIndex) override {
        Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Track>(*getOwnerView()), state,
                           insertIndex, undoManager);
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
            tracks = state.getChildWithName(IDs::TRACKS);
            masterTrack = tracks.getChildWithName(IDs::MASTER_TRACK);
        }
        jassert (state.hasType(IDs::PROJECT));
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
        Helpers::deleteSelectedItems(*getOwnerView(), undoManager);
    }

    ValueTree createDefaultProject() {
        state = ValueTree(IDs::PROJECT);
        state.setProperty(IDs::name, "My First Project", nullptr);
        Helpers::createUuidProperty(state);

        tracks = ValueTree(IDs::TRACKS);
        Helpers::createUuidProperty(tracks);
        tracks.setProperty(IDs::name, "Tracks", nullptr);

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

        masterTrack = ValueTree(IDs::MASTER_TRACK);
        Helpers::createUuidProperty(masterTrack);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        createAndAddProcessor(masterTrack, MixerChannelProcessor::name(), false);

        tracks.addChild(masterTrack, -1, nullptr);

        state.addChild(tracks, -1, nullptr);

        return state;
    }

    ValueTree createAndAddTrack(bool undoable=true) {
        int numTracks = this->getNumSubItems();

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

        const ValueTree &mixerChannelProcessor = track.getChildWithProperty(IDs::name, MixerChannelProcessor::name());
        // Insert new processors _right before_ the first g&b processor, or at the end if there isn't one.
        int insertIndex = mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : -1;
        int slot = 0;
        if (insertIndex == -1 && name == MixerChannelProcessor::name()) {
            slot = NUM_VISIBLE_TRACK_SLOTS - 1;
        } else if (track.getNumChildren() > 1) {
            slot = int(track.getChild(track.getNumChildren() - 2).getProperty(IDs::PROCESSOR_SLOT)) + 1;
        }

        processor.setProperty(IDs::PROCESSOR_SLOT, slot, nullptr);
        track.addChild(processor, insertIndex, undoable ? &undoManager : nullptr);

        return processor;
    }

    void makeSlotsValid(const ValueTree& parent) {
        std::vector<int> slots;
        for (int i = 0; i < parent.getNumChildren(); i++) {
            const ValueTree& child = parent.getChild(i);
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
        for (int i = 0; i < parent.getNumChildren(); i++) {
            ValueTree child = parent.getChild(i);
            if (child.hasType(IDs::PROCESSOR)) {
                child.setProperty(IDs::PROCESSOR_SLOT, *(iterator++), nullptr);
            }
        }
    }

    // TODO consider moving this (and a bunch of other similar logic) to ValueTreeItems::Track
    // and using a 'Project' here instead of 'ValueTree' project state
    int getParentIndexForProcessor(const ValueTree &parent, const ValueTree &processorState) {
        for (int i = 0; parent.getNumChildren(); i++) {
            const ValueTree &otherProcessorState = parent.getChild(i);
            if (processorState == otherProcessorState)
                continue;
            if (otherProcessorState.hasType(IDs::PROCESSOR) &&
                int(otherProcessorState.getProperty(IDs::PROCESSOR_SLOT)) > int(processorState.getProperty(IDs::PROCESSOR_SLOT))) {
                return parent.indexOf(otherProcessorState);
            }
        }

        // TODO in this and other places, we assume processers are the only type of track child.
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

    const static int NUM_VISIBLE_TRACK_SLOTS = 8;
private:
    ValueTree tracks;
    ValueTree masterTrack;
    ValueTree selectedTrack;
    ValueTree selectedProcessor;
};


inline ValueTreeItem *createValueTreeItemForType(const ValueTree &v, UndoManager &um) {
    if (v.hasType(IDs::PROJECT)) return new Project(v, um);
    if (v.hasType(IDs::TRACKS)) return new Tracks(v, um);
    if (v.hasType(IDs::MASTER_TRACK)) return new MasterTrack(v, um);
    if (v.hasType(IDs::TRACK)) return new Track(v, um);
    if (v.hasType(IDs::CLIP)) return new Clip(v, um);
    if (v.hasType(IDs::PROCESSOR)) return new Processor(v, um);

    //jassertfalse;
    return nullptr;
}
