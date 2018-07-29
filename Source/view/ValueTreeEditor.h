#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <view/SelectionPanel.h>
#include <processors/ProcessorManager.h>
#include "UiColours.h"

class ValueTreeEditor : public Component,
                        public DragAndDropContainer,
                        private ProjectChangeListener,
                        private Button::Listener,
                        private Timer {
public:
    ValueTreeEditor(const ValueTree &state, UndoManager &undoManager, Project& project, ProcessorGraph &audioGraphBuilder)
            : undoManager(undoManager), project(project) {
        addAndMakeVisible(treeView);
        treeView.setColour(TreeView::backgroundColourId, getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));

        treeView.setDefaultOpenness(true);
        treeView.setMultiSelectEnabled(true);

        treeView.setRootItem(&project);
        project.addChangeListener(this);
        addAndMakeVisible(*(selectionPanel = std::make_unique<SelectionPanel>(project, audioGraphBuilder)));

        Utilities::visitComponents({&undoButton, &redoButton, &createTrackButton, &addProcessorButton},
                                   [this](Component *c) { addAndMakeVisible(c); });

        undoButton.addListener(this);
        redoButton.addListener(this);
        createTrackButton.addListener(this);
        addProcessorButton.addListener(this);

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
        addProcessorButton.setBounds(buttons.removeFromLeft(120));

        r.removeFromBottom(4);
        selectionPanel->setBounds(r);
    }

    void buttonClicked(Button *b) override {
        if (b == &undoButton) {
            undoManager.undo();
        } else if (b == &redoButton) {
            undoManager.redo();
        } else if (b == &createTrackButton) {
            project.createAndAddTrack();
        } else if (b == &addProcessorButton) {
            if (project.getSelectedTrack().isValid()) {
                addProcessorMenu = std::make_unique<PopupMenu>();
                project.addPluginsToMenu(*addProcessorMenu, project.getSelectedTrack());
                addProcessorMenu->showMenuAsync({}, ModalCallbackFunction::create([this](int r) {
                    if (auto *description = project.getChosenType(r)) {
                        project.createAndAddProcessor(*description);
                    }
                }));
            }
        }
    }

    void sendSelectMessageForFirstSelectedItem() {
        if (treeView.getNumSelectedItems() > 0) {
            TreeViewItem *selectedItem = treeView.getSelectedItem(0);
            selectedItem->itemSelectionChanged(false);
            selectedItem->itemSelectionChanged(true);
        }
    }

    void itemSelected(const ValueTree& item) override {
        const ValueTree &selectedTrack = project.getSelectedTrack();
        addProcessorButton.setVisible(selectedTrack.isValid());
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedTrack()) {
            addProcessorButton.setVisible(false);
        }
    }

private:
    TreeView treeView;
    TextButton undoButton{"Undo"}, redoButton{"Redo"}, createTrackButton{"Add Track"};

    TextButton addProcessorButton{"Add Processor"};

    UndoManager &undoManager;
    Project &project;

    std::unique_ptr<SelectionPanel> selectionPanel;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void timerCallback() override {
        undoManager.beginNewTransaction();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeEditor)
};
