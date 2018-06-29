#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <view/SelectionPanel.h>

class ValueTreeEditor : public Component,
                        public DragAndDropContainer,
                        private ProjectChangeListener,
                        private Button::Listener,
                        private ComboBox::Listener,
                        private Timer {
public:
    ValueTreeEditor(const ValueTree &state, UndoManager &undoManager, Project& project, AudioGraphBuilder &audioGraphBuilder) : undoManager(undoManager), project(project) {
        addAndMakeVisible(tree);
        tree.setColour(TreeView::backgroundColourId,
                       getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));

        tree.setDefaultOpenness(true);
        tree.setMultiSelectEnabled(true);

        tree.setRootItem(&project);
        project.addChangeListener(this);
        addAndMakeVisible(*(selectionPanel = std::make_unique<SelectionPanel>(project, audioGraphBuilder)));

        Utilities::visitComponents({&undoButton, &redoButton, &createTrackButton, &createProcessorComboBox},
                                   [this](Component *c) { addAndMakeVisible(c); });

        undoButton.addListener(this);
        redoButton.addListener(this);
        createTrackButton.addListener(this);
        createProcessorComboBox.addListener(this);
        createProcessorComboBox.addItemList(processorNames, 1);
        createProcessorComboBox.setTextWhenNothingSelected("Create processor");
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

        buttons.removeFromLeft(6);
        createTrackButton.setBounds(buttons.removeFromLeft(100));

        buttons.removeFromLeft(6);
        createProcessorComboBox.setBounds(buttons.removeFromLeft(200));

        r.removeFromBottom(4);
        selectionPanel->setBounds(r.removeFromBottom(120));

        r.removeFromBottom(4);
        tree.setBounds(r);
    }

    void deleteSelectedItems() {
        auto selectedItems(Helpers::getSelectedAndDeletableTreeViewItems<ValueTreeItem>(tree));

        for (int i = selectedItems.size(); --i >= 0;) {
            ValueTree &v = *selectedItems.getUnchecked(i);

            if (v.getParent().isValid())
                v.getParent().removeChild(v, &undoManager);
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
        if (b == &undoButton) {
            undoManager.undo();
        } else if (b == &redoButton) {
            undoManager.redo();
        } else if (b == &createTrackButton) {
            project.createAndAddTrack();
        }
    }

    void comboBoxChanged(ComboBox* cb) override  {
        if (cb == &createProcessorComboBox) {
            if (selectedTrack.isValid()) {
                project.createAndAddProcessor(selectedTrack, processorNames[cb->getSelectedId() - 1]);
            }
        }
        cb->setSelectedItemIndex(-1, dontSendNotification); // don't keep displaying the selected item
    }

    void sendSelectMessageForFirstSelectedItem() {
        if (tree.getNumSelectedItems() > 0) {
            TreeViewItem *selectedItem = tree.getSelectedItem(0);
            selectedItem->itemSelectionChanged(false);
            selectedItem->itemSelectionChanged(true);
        }
    }

    void itemSelected(ValueTree item) override {
        if (item.hasType(IDs::TRACK)) {
            selectedTrack = item;
            createProcessorComboBox.setVisible(true);
        } else {
            createProcessorComboBox.setVisible(false);
        }
    }

    void itemRemoved(ValueTree item) override {
        if (item.hasType(IDs::TRACK)) {
            if (selectedTrack.isValid() && selectedTrack[IDs::uuid] == item[IDs::uuid]) {
                selectedTrack = ValueTree();
                createProcessorComboBox.setVisible(false);
            }
        }
    }

private:
    TreeView tree;
    ValueTree selectedTrack;
    StringArray processorNames { GainProcessor::name(), SineBank::name() };
    TextButton undoButton{"Undo"}, redoButton{"Redo"}, createTrackButton{"Create Track"};

    ComboBox createProcessorComboBox{"Create Processor"};

    UndoManager &undoManager;
    Project &project;

    std::unique_ptr<SelectionPanel> selectionPanel;

    void timerCallback() override {
        undoManager.beginNewTransaction();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeEditor)
};
