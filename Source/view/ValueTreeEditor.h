#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <view/SelectionPanel.h>

class ValueTreeEditor : public Component,
                       public DragAndDropContainer,
                       private Button::Listener,
                       private Timer {
public:
    ValueTreeEditor(const ValueTree &state, UndoManager &undoManager, Project& project, AudioGraphBuilder &audioGraphBuilder) : undoManager(undoManager), project(project) {
        addAndMakeVisible(tree);
        tree.setColour(TreeView::backgroundColourId,
                       getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));

        tree.setDefaultOpenness(true);
        tree.setMultiSelectEnabled(true);

        tree.setRootItem(&project);

        addAndMakeVisible(*(selectionPanel = std::make_unique<SelectionPanel>(project, audioGraphBuilder)));

        addAndMakeVisible(undoButton);
        addAndMakeVisible(redoButton);
        addAndMakeVisible(createTrackButton);

        undoButton.addListener(this);
        redoButton.addListener(this);
        createTrackButton.addListener(this);

        startTimer(500);
        setSize(800, 600);
    }

    ~ValueTreeEditor() override {
        tree.setRootItem(nullptr);
    }

    void resized() override {
        Rectangle<int> r(getLocalBounds().reduced(4));

        Rectangle<int> buttons(r.removeFromBottom(22));
        undoButton.setBounds(buttons.removeFromLeft(100));
        buttons.removeFromLeft(6);
        redoButton.setBounds(buttons.removeFromLeft(100));

        createTrackButton.setBounds(buttons.removeFromLeft(100));

        r.removeFromBottom(4);
        selectionPanel->setBounds(r.removeFromBottom(120));

        r.removeFromBottom(4);
        tree.setBounds(r);
    }

    void deleteSelectedItems() {
        auto selectedItems(Helpers::getSelectedAndDeletableTreeViewItems<ValueTreeItem>(tree));

        if (selectedItems.isEmpty())
            return;

        ValueTree itemToSelectAfterDelete = selectedItems.getFirst()->getSibling(-1);
        if (!itemToSelectAfterDelete.isValid()) {
            itemToSelectAfterDelete = selectedItems.getLast()->getSibling(1);
            if (!itemToSelectAfterDelete.isValid()) {
                itemToSelectAfterDelete = selectedItems.getFirst()->getParent();
            }
        }

        for (int i = selectedItems.size(); --i >= 0;) {
            ValueTree &v = *selectedItems.getUnchecked(i);

            if (v.getParent().isValid())
                v.getParent().removeChild(v, &undoManager);
        }

        if (itemToSelectAfterDelete.isValid()) {
            itemToSelectAfterDelete.setProperty(IDs::selected, true, nullptr);
        }
    }

    bool keyPressed(const KeyPress &key) override {
        if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
            deleteSelectedItems();
            return true;
        }

        if (key == KeyPress('z', ModifierKeys::commandModifier, 0)) {
            undoManager.undo();
            return true;
        }

        if (key == KeyPress('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0)) {
            undoManager.redo();
            return true;
        }

        return Component::keyPressed(key);
    }

    void buttonClicked(Button *b) override {
        if (b == &undoButton)
            undoManager.undo();
        else if (b == &redoButton)
            undoManager.redo();
        else if (b == &createTrackButton) {
            project.createAndAddTrack();
        }
    }

    void sendSelectMessageForFirstSelectedItem() {
        if (tree.getNumSelectedItems() > 0) {
            TreeViewItem *selectedItem = tree.getSelectedItem(0);
            // cycle to make sure state changes
            selectedItem->itemSelectionChanged(false);
            selectedItem->itemSelectionChanged(true);
        }
    }

private:
    TreeView tree;

    TextButton undoButton{"Undo"}, redoButton{"Redo"}, createTrackButton {"Create Track"};
    UndoManager &undoManager;
    Project &project;

    std::unique_ptr<SelectionPanel> selectionPanel;

    void timerCallback() override {
        undoManager.beginNewTransaction();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeEditor)
};
