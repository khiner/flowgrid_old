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
        addAndMakeVisible(treeView);
        treeView.setColour(TreeView::backgroundColourId,
                       getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));

        treeView.setDefaultOpenness(true);
        treeView.setMultiSelectEnabled(true);

        treeView.setRootItem(&project);
        project.addChangeListener(this);
        addAndMakeVisible(*(selectionPanel = std::make_unique<SelectionPanel>(project, audioGraphBuilder)));

        Utilities::visitComponents({&undoButton, &redoButton, &createTrackButton, &addProcessorComboBox},
                                   [this](Component *c) { addAndMakeVisible(c); });

        undoButton.addListener(this);
        redoButton.addListener(this);
        createTrackButton.addListener(this);
        addProcessorComboBox.addListener(this);
        addProcessorComboBox.addItemList(processorNames, 1);
        addProcessorComboBox.setTextWhenNothingSelected("Create processor");
        startTimer(500);
        setSize(800, 600);
    }

    ~ValueTreeEditor() override {
        treeView.setRootItem(nullptr);
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
        addProcessorComboBox.setBounds(buttons.removeFromLeft(200));

        r.removeFromBottom(4);
        selectionPanel->setBounds(r.removeFromBottom(120));

        r.removeFromBottom(4);
        treeView.setBounds(r);
    }

    bool keyPressed(const KeyPress &key) override {
        if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
            Helpers::deleteSelectedItems(treeView, undoManager);
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
        if (cb == &addProcessorComboBox) {
            if (project.getSelectedTrack().isValid()) {
                project.createAndAddProcessor(processorNames[cb->getSelectedId() - 1]);
            }
        }
        cb->setSelectedItemIndex(-1, dontSendNotification); // don't keep displaying the selected item
    }

    void sendSelectMessageForFirstSelectedItem() {
        if (treeView.getNumSelectedItems() > 0) {
            TreeViewItem *selectedItem = treeView.getSelectedItem(0);
            selectedItem->itemSelectionChanged(false);
            selectedItem->itemSelectionChanged(true);
        }
    }

    void itemSelected(ValueTree item) override {
        addProcessorComboBox.setVisible(project.getSelectedTrack().isValid());
    }

    void itemRemoved(ValueTree item) override {
        if (item == project.getSelectedTrack()) {
            addProcessorComboBox.setVisible(false);
        }
    }

private:
    TreeView treeView;
    StringArray processorNames { GainProcessor::name(), SineBank::name() };
    TextButton undoButton{"Undo"}, redoButton{"Redo"}, createTrackButton{"Add Track"};

    ComboBox addProcessorComboBox{"Add Processor"};

    UndoManager &undoManager;
    Project &project;

    std::unique_ptr<SelectionPanel> selectionPanel;

    void timerCallback() override {
        undoManager.beginNewTransaction();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeEditor)
};
