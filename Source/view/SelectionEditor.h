#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorManager.h>
#include "UiColours.h"
#include "processor_editor/ProcessorEditor.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        public ChangeListener,
                        private ProjectChangeListener,
                        private Button::Listener {
public:
    SelectionEditor(Project& project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder) {
        project.addProjectChangeListener(this);
        addChildComponent((processorEditor = std::make_unique<ProcessorEditor>()).get());
        Utilities::visitComponents({&undoButton, &redoButton, &addProcessorButton},
                                   [this](Component *c) { addAndMakeVisible(c); });

        undoButton.addListener(this);
        redoButton.addListener(this);
        addProcessorButton.addListener(this);

        setSize(800, 600);
    }

    void resized() override {
        Rectangle<int> r(getLocalBounds().reduced(4));

        Rectangle<int> buttons(r.removeFromBottom(22));
        undoButton.setBounds(buttons.removeFromLeft(100));
        buttons.removeFromLeft(6);
        redoButton.setBounds(buttons.removeFromLeft(100));

        buttons.removeFromLeft(6);
        addProcessorButton.setBounds(buttons.removeFromLeft(120));

        if (processorEditor != nullptr) {
            processorEditor->setBounds(r);
        }
    }

    void buttonClicked(Button *b) override {
        if (b == &undoButton) {
            getCommandManager().invokeDirectly(CommandIDs::undo, false);
        } else if (b == &redoButton) {
            getCommandManager().invokeDirectly(CommandIDs::redo, false);
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

    void itemSelected(const ValueTree& item) override {
        addProcessorButton.setVisible(item.hasType(IDs::PROCESSOR) || item.hasType(IDs::TRACK) || item.hasType(IDs::MASTER_TRACK));

        if (item.hasType(IDs::PROCESSOR)) {
            if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(item)) {
                processorEditor->setProcessor(processorWrapper);
                processorEditor->setVisible(true);
            }
        } else if ((item.hasType(IDs::TRACK) || item.hasType(IDs::MASTER_TRACK)) && item.getNumChildren() == 0) {
            processorEditor->setVisible(false);
            processorEditor->setProcessor(nullptr);
        }

        resized();
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedTrack()) {
            addProcessorButton.setVisible(false);
            itemSelected(ValueTree());
        } else if (item == project.getSelectedProcessor()) {
            itemSelected(ValueTree());
        }
    }

    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (auto* undoManager = dynamic_cast<UndoManager *>(source)) {
            undoButton.setEnabled(undoManager->canUndo());
            redoButton.setEnabled(undoManager->canRedo());
        }
    }

private:
    TextButton undoButton{"Undo"}, redoButton{"Redo"};

    TextButton addProcessorButton{"Add Processor"};

    std::unique_ptr<ProcessorEditor> processorEditor {};

    Project &project;
    ProcessorGraph &audioGraphBuilder;

    std::unique_ptr<PopupMenu> addProcessorMenu;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionEditor)
};
