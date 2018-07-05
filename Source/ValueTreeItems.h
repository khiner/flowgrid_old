#pragma once

#include <processors/SineBank.h>
#include <processors/GainProcessor.h>
#include "view/ColourChangeButton.h"
#include "Identifiers.h"

class ValueTreeItem;

class ProjectChangeListener {
public:
    virtual void itemSelected(ValueTree) = 0;
    virtual void itemRemoved(ValueTree) = 0;
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

    inline void moveItems(TreeView &treeView, const OwnedArray<ValueTree> &items,
                          ValueTree newParent, int insertIndex, UndoManager &undoManager) {
        if (items.isEmpty())
            return;

        std::unique_ptr<XmlElement> oldOpenness(treeView.getOpennessState(false));

        for (int i = items.size(); --i >= 0;) {
            ValueTree &v = *items.getUnchecked(i);

            if (v.getParent().isValid() && newParent != v && !newParent.isAChildOf(v)) {
                if (v.getParent() == newParent) {
                    if (newParent.indexOf(v) < insertIndex) {
                        --insertIndex;
                    }
                    v.getParent().moveChild(v.getParent().indexOf(v), insertIndex, &undoManager);
                } else {
                    v.getParent().removeChild(v, &undoManager);
                    newParent.addChild(v, insertIndex, &undoManager);
                }
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


class Project : public ValueTreeItem,
                public ProjectChangeBroadcaster {
public:
    Project(const ValueTree &v, UndoManager &um)
            : ValueTreeItem(v, um) {
        if (!state.isValid()) {
            state = createDefaultProject();
        }
        jassert (state.hasType(IDs::PROJECT));
    }

    ValueTree getSelectedTrack() {
        return selectedTrack;
    }

    ValueTree getSelectedProcessor() {
        return selectedProcessor;
    }

    StringArray& getProcessorNames() {
        return processorNames;
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

        for (int tn = 0; tn < 1; ++tn) {
            ValueTree track = createAndAddTrack(false);
            ValueTree processor = createAndAddProcessor(track, SineBank::name(), false);

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

        ValueTree masterTrack(IDs::MASTER_TRACK);
        Helpers::createUuidProperty(masterTrack);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        createAndAddProcessor(masterTrack, GainProcessor::name(), false);

        state.addChild(masterTrack, -1, nullptr);

        return state;
    }

    ValueTree createAndAddTrack(bool undoable=true) {
        int numTracks = this->getNumSubItems();

        ValueTree track(IDs::TRACK);
        const String trackName("Track " + String(numTracks + 1));
        Helpers::createUuidProperty(track);
        track.setProperty(IDs::colour, Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);
        track.setProperty(IDs::name, trackName, nullptr);

        state.addChild(track, -1, undoable ? &undoManager : nullptr);

        return track;
    }

    ValueTree createAndAddProcessor(const String& name, bool undoable=true) {
        ValueTree selectedTrack = getSelectedTrack();
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
        track.addChild(processor, -1, undoable ? &undoManager : nullptr);

        return processor;
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

private:
    ValueTree selectedTrack;
    ValueTree selectedProcessor;

    StringArray processorNames { GainProcessor::name(), SineBank::name() };
};


inline ValueTreeItem *createValueTreeItemForType(const ValueTree &v, UndoManager &um) {
    if (v.hasType(IDs::PROJECT)) return new Project(v, um);
    if (v.hasType(IDs::MASTER_TRACK)) return new MasterTrack(v, um);
    if (v.hasType(IDs::TRACK)) return new Track(v, um);
    if (v.hasType(IDs::CLIP)) return new Clip(v, um);
    if (v.hasType(IDs::PROCESSOR)) return new Processor(v, um);

    //jassertfalse;
    return nullptr;
}
