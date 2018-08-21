#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorManager.h>
#include "UiColours.h"
#include "processor_editor/ProcessorEditor.h"
#include "view/context_pane/ContextPane.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        public ChangeListener,
                        private Button::Listener,
                        private Utilities::ValueTreePropertyChangeListener {
public:
    SelectionEditor(Project& project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder), contextPane(project) {
        project.getState().addListener(this);
        addAndMakeVisible(contextPane);
        addChildComponent((processorEditor = std::make_unique<ProcessorEditor>()).get());
        Utilities::visitComponents({&undoButton, &redoButton, &addProcessorButton},
                                   [this](Component *c) { addAndMakeVisible(c); });

        undoButton.addListener(this);
        redoButton.addListener(this);
        addProcessorButton.addListener(this);

        setSize(800, 600);
    }

    ~SelectionEditor() override {
        project.getState().removeListener(this);
    }

    void resized() override {
        Rectangle<int> r(getLocalBounds().reduced(4));

        Rectangle<int> buttons(r.removeFromBottom(22));
        undoButton.setBounds(buttons.removeFromLeft(100));
        buttons.removeFromLeft(6);
        redoButton.setBounds(buttons.removeFromLeft(100));

        buttons.removeFromLeft(6);
        addProcessorButton.setBounds(buttons.removeFromLeft(120));

        contextPane.setBounds(r.removeFromBottom(400).reduced(4));
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
                        project.createAndAddProcessor(*description, &project.getUndoManager());
                    }
                }));
            }
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int) override {
        if (tree.hasType(IDs::TRACK) || tree.hasType(IDs::PROCESSOR)) {
            if (!project.getSelectedTrack().isValid()) {
                addProcessorButton.setVisible(false);
                processorEditor->setVisible(false);
                processorEditor->setProcessor(nullptr);
            }
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

    ContextPane contextPane;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected && tree[IDs::selected]) {
            addProcessorButton.setVisible(tree.hasType(IDs::PROCESSOR) || tree.hasType(IDs::TRACK));

            if (tree.hasType(IDs::PROCESSOR)) {
                if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(tree)) {
                    processorEditor->setProcessor(processorWrapper);
                    processorEditor->setVisible(true);
                }
            } else {
                processorEditor->setVisible(false);
                processorEditor->setProcessor(nullptr);
            }

            resized();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionEditor)
};
