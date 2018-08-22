#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorManager.h>
#include "processor_editor/ProcessorEditor.h"
#include "view/context_pane/ContextPane.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        private Button::Listener,
                        private Utilities::ValueTreePropertyChangeListener {
public:
    SelectionEditor(Project& project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder), contextPane(project) {
        project.getState().addListener(this);
        addAndMakeVisible(contextPane);
        addChildComponent((processorEditor = std::make_unique<ProcessorEditor>()).get());
        addAndMakeVisible(addProcessorButton);

        addProcessorButton.addListener(this);

        setSize(800, 600);
    }

    ~SelectionEditor() override {
        project.getState().removeListener(this);
    }

    void resized() override {
        auto r(getLocalBounds().reduced(4));
        auto buttons(r.removeFromTop(22));
        addProcessorButton.setBounds(buttons.removeFromLeft(120));

        contextPane.setBounds(r.removeFromBottom(400).reduced(4));
        if (processorEditor != nullptr) {
            r.removeFromTop(8);
            processorEditor->setBounds(r.reduced(4));
        }
    }

    void buttonClicked(Button *b) override {
        if (b == &addProcessorButton) {
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

private:
    TextButton addProcessorButton{"Add Processor"};

    std::unique_ptr<ProcessorEditor> processorEditor {};

    Project &project;
    ProcessorGraph &audioGraphBuilder;

    ContextPane contextPane;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected && tree[IDs::selected]) {
            addProcessorButton.setVisible(tree.hasType(IDs::PROCESSOR) || tree.hasType(IDs::TRACK));

            const auto& selectedProcessor = tree.hasType(IDs::PROCESSOR) ? tree : project.findSelectedProcessorForTrack(tree);
            if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(selectedProcessor)) {
                processorEditor->setProcessor(processorWrapper);
                processorEditor->setVisible(true);
            } else {
                processorEditor->setVisible(false);
                processorEditor->setProcessor(nullptr);
            }

            resized();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionEditor)
};
