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
                        private ValueTree::Listener {
public:
    SelectionEditor(Project& project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder), contextPane(project) {
        project.getState().addListener(this);
        addAndMakeVisible(contextPaneViewport);
        addAndMakeVisible(addProcessorButton);
        contextPaneViewport.setViewedComponent(&contextPane, false);
        addProcessorButton.addListener(this);
        // todo viewport for processor editors
    }

    ~SelectionEditor() override {
        project.getState().removeListener(this);
    }

    void paint(Graphics& g) override { // divider lines
        g.setColour(findColour(TextEditor::backgroundColourId));
        g.drawLine(1, 0, 1, getHeight(), 2);
        g.drawLine(0, contextPaneViewport.getY(), getWidth(), contextPaneViewport.getY(), 2);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(4);
        auto buttons = r.removeFromTop(22);
        buttons.removeFromLeft(4);
        addProcessorButton.setBounds(buttons.removeFromLeft(120));
        contextPaneViewport.setBounds(r.removeFromBottom(250).reduced(1));
        for (auto* editor : processorEditors) {
            if (editor->isVisible()) {
                editor->setBounds(r.removeFromTop(160).reduced(4));
            }
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

private:
    TextButton addProcessorButton{"Add Processor"};

    OwnedArray<ProcessorEditor> processorEditors;

    Project &project;
    ProcessorGraph &audioGraphBuilder;

    Viewport contextPaneViewport;
    ContextPane contextPane;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void refreshProcessors(const ValueTree& singleProcessorToRefresh={}) {
        const ValueTree &selectedTrack = project.getSelectedTrack();
        if (!selectedTrack.isValid()) {
            for (auto* editor : processorEditors) {
                editor->setVisible(false);
                editor->setProcessor(nullptr);
            }
        } else if (singleProcessorToRefresh.isValid()) {
            assignProcessorToEditor(singleProcessorToRefresh);
        } else {
            for (int processorSlot = 0; processorSlot < processorEditors.size(); processorSlot++) {
                const auto &processor = selectedTrack.getChildWithProperty(IDs::processorSlot, processorSlot);
                assignProcessorToEditor(processor, processorSlot);
            }
        }
        resized();
    }

    void assignProcessorToEditor(const ValueTree &processor, int processorSlot=-1) const {
        auto* processorEditor = processorEditors.getUnchecked(processorSlot != -1 ? processorSlot : int(processor[IDs::processorSlot]));
        if (processor.isValid()) {
            if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(processor)) {
                processorEditor->setProcessor(processorWrapper);
                processorEditor->setVisible(true);
            }
        } else {
            processorEditor->setVisible(false);
            processorEditor->setProcessor(nullptr);
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasProperty(IDs::PROCESSOR) && project.getSelectedTrack() == parent) {
            assignProcessorToEditor(child);
            resized();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::TRACK) || child.hasType(IDs::PROCESSOR)) {
            refreshProcessors(); // todo can improve efficiency by only removing this processor editor
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected && tree[IDs::selected]) {
            addProcessorButton.setVisible(tree.hasType(IDs::PROCESSOR) || tree.hasType(IDs::TRACK));

            const auto& selectedProcessor = tree.hasType(IDs::PROCESSOR) ? tree : project.findSelectedProcessorForTrack(tree);
            refreshProcessors();
        } else if (i == IDs::numProcessorSlots || i == IDs::numMasterProcessorSlots) {
            int numProcessorSlots = jmax(int(tree[IDs::numProcessorSlots]), int(tree[IDs::numMasterProcessorSlots]));
            while (processorEditors.size() < numProcessorSlots) {
                auto* processorEditor = new ProcessorEditor();
                addChildComponent(processorEditor);
                processorEditors.add(processorEditor);
            }
            processorEditors.removeLast(processorEditors.size() - numProcessorSlots);
        } else if (i == IDs::processorSlot) {
            refreshProcessors();
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionEditor)
};
