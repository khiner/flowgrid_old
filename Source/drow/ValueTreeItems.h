#pragma once

#include "ColourChangeButton.h"
#include "Identifiers.h"

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

    inline ValueTree createDefaultProject() {
        ValueTree project(IDs::PROJECT);
        project.setProperty(IDs::name, "My First Project", nullptr);

        Helpers::createUuidProperty(project);

        for (int tn = 0; tn < 1; ++tn) {
            ValueTree t(IDs::TRACK);
            const String trackName("Track " + String(tn + 1));
            t.setProperty(IDs::colour, Colour::fromHSV((1.0f / 8.0f) * tn, 0.65f, 0.65f, 1.0f).toString(), nullptr);
            t.setProperty(IDs::name, trackName, nullptr);
            t.setProperty(IDs::selected, true, nullptr);
            Helpers::createUuidProperty(t);

            ValueTree i(IDs::PROCESSOR);
            Helpers::createUuidProperty(i);
            i.setProperty(IDs::name, IDs::SINE_BANK_PROCESSOR.toString(), nullptr);
            i.setProperty(IDs::selected, true, nullptr);
            t.addChild(i, -1, nullptr);

            for (int cn = 0; cn < 3; ++cn) {
                ValueTree c(IDs::CLIP);
                Helpers::createUuidProperty(c);
                c.setProperty(IDs::name, trackName + ", Clip " + String(cn + 1), nullptr);
                c.setProperty(IDs::start, cn, nullptr);
                c.setProperty(IDs::length, 1.0, nullptr);
                c.setProperty(IDs::selected, false, nullptr);

                t.addChild(c, -1, nullptr);
            }

            project.addChild(t, -1, nullptr);
        }

        return project;
    }
}

class ValueTreeItem;

class ProjectChangeListener {
public:
    virtual void itemSelected(ValueTreeItem*) = 0;
};

// Adapted from JUCE::ChangeBroadcaster to support messaging specific changes to the project.
class ProjectChangeBroadcaster {
public:
    ProjectChangeBroadcaster() = default;

    virtual ~ProjectChangeBroadcaster() {};

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

    /** Removes all listeners from the list. */
    void removeAllChangeListeners() {
        // Listeners can only be safely removed when the event thread is locked
        // You can  use a MessageManagerLock if you need to call this from another thread.
        jassert (MessageManager::getInstance()->currentThreadHasLockedMessageManager());

        changeListeners.clear();
    }

    //==============================================================================
    /** Causes an asynchronous change message to be sent to all the registered listeners.

        The message will be delivered asynchronously by the main message thread, so this
        method will return immediately. To call the listeners synchronously use
        sendSynchronousChangeMessage().
    */
    void sendItemSelectedMessage(ValueTreeItem *item) {
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            changeListeners.call (&ProjectChangeListener::itemSelected, item);
        } else {
            MessageManager::callAsync([this, item] { changeListeners.call(&ProjectChangeListener::itemSelected, item); });
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
    ValueTreeItem(const ValueTree &v, UndoManager &um)
            : state(v), undoManager(um) {
        state.addListener(this);
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
        if (isNowSelected) {
            if (auto *ov = getOwnerView()) {
                if (auto *cb = dynamic_cast<ProjectChangeBroadcaster *> (ov->getRootItem())) {
                    cb->sendItemSelectedMessage(this);
                }
            }
        }

    }

    var getDragSourceDescription() override {
        return state.getType().toString();
    }

protected:
    ValueTree state;
    UndoManager &undoManager;

    void valueTreePropertyChanged(ValueTree & tree, const Identifier &identifier) override {
        if (tree.getType() != IDs::PARAM) // TODO revisit if I ever stop using AudioProcessorValueTreeStates for my processors
            repaintItem();
    }

    void valueTreeChildAdded(ValueTree &parentTree, ValueTree &) override { treeChildrenChanged(parentTree); }

    void valueTreeChildRemoved(ValueTree &parentTree, ValueTree &, int) override { treeChildrenChanged(parentTree); }

    void valueTreeChildOrderChanged(ValueTree &parentTree, int, int) override { treeChildrenChanged(parentTree); }

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

    ~Processor() {

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
    Track(const ValueTree &v, UndoManager &um)
            : ValueTreeItem(v, um) {
        jassert (state.hasType(IDs::TRACK));
    }

    void itemSelectionChanged(bool isNowSelected) override {
        ValueTreeItem::itemSelectionChanged(isNowSelected);
    }

    bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails) override {
        return dragSourceDetails.description == IDs::CLIP.toString() ||
               dragSourceDetails.description == IDs::PROCESSOR.toString();
    }

    void itemDropped(const DragAndDropTarget::SourceDetails &, int insertIndex) override {
        Helpers::moveItems(*getOwnerView(), Helpers::getSelectedTreeViewItems<Clip>(*getOwnerView()), state,
                           insertIndex, undoManager);
    }
};


class Project : public ValueTreeItem,
             public ProjectChangeBroadcaster {
public:
    Project(const ValueTree &v, UndoManager &um)
            : ValueTreeItem(v, um) {
        jassert (state.hasType(IDs::PROJECT));
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


inline ValueTreeItem *createValueTreeItemForType(const ValueTree &v, UndoManager &um) {
    if (v.hasType(IDs::PROJECT)) return new Project(v, um);
    if (v.hasType(IDs::TRACK)) return new Track(v, um);
    if (v.hasType(IDs::CLIP)) return new Clip(v, um);
    if (v.hasType(IDs::PROCESSOR)) return new Processor(v, um);

    //jassertfalse;
    return nullptr;
}
