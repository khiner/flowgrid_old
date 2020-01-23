#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorManager.h>
#include "processor_editor/ProcessorEditor.h"
#include "view/context_pane/ContextPane.h"
#include "view/CustomColourIds.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        private Button::Listener,
                        private ValueTree::Listener,
                        private ProcessorLifecycleListener {
public:
    SelectionEditor(Project& project, ProcessorGraph &audioGraphBuilder)
            : project(project), tracksManager(project.getTracksManager()),
              viewManager(project.getViewStateManager()),
              audioGraphBuilder(audioGraphBuilder), contextPane(project) {
        this->project.getState().addListener(this);
        tracksManager.addProcessorLifecycleListener(this);

        addAndMakeVisible(addProcessorButton);
        addAndMakeVisible(processorEditorsViewport);
        addAndMakeVisible(contextPaneViewport);
        unfocusOverlay.setFill(findColour(CustomColourIds::unfocusedOverlayColourId));
        addChildComponent(unfocusOverlay);

        addProcessorButton.addListener(this);
        processorEditorsViewport.setViewedComponent(&processorEditorsComponent, false);
        processorEditorsViewport.setScrollBarsShown(true, false);
        contextPaneViewport.setViewedComponent(&contextPane, false);
        addMouseListener(this, true);
    }

    ~SelectionEditor() override {
        tracksManager.removeProcessorLifecycleListener(this);
        project.getState().removeListener(this);
    }

    void mouseDown(const MouseEvent& event) override {
        viewManager.focusOnEditorPane();
        if (auto* processorEditor = dynamic_cast<ProcessorEditor*>(event.originalComponent)) {
            tracksManager.selectProcessor(processorEditor->getProcessorState());
        }
    }

    void paint(Graphics& g) override {
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
        processorEditorsViewport.setBounds(r);
        unfocusOverlay.setRectangle(processorEditorsViewport.getBounds().toFloat());

        r.removeFromRight(8);
        processorEditorsComponent.setBounds(0, 0, r.getWidth(), PROCESSOR_EDITOR_HEIGHT * tracksManager.getSelectedTrack().getNumChildren());
        r = processorEditorsComponent.getBounds();
        for (auto* editor : processorEditors) {
            if (editor->isVisible())
                editor->setBounds(r.removeFromTop(PROCESSOR_EDITOR_HEIGHT).reduced(4));
        }
    }

    void buttonClicked(Button *b) override {
        if (b == &addProcessorButton) {
            const auto& selectedTrack = tracksManager.getSelectedTrack();
            if (selectedTrack.isValid()) {
                addProcessorMenu = std::make_unique<PopupMenu>();
                project.addPluginsToMenu(*addProcessorMenu, selectedTrack);
                addProcessorMenu->showMenuAsync({}, ModalCallbackFunction::create([this](int r) {
                    if (auto *description = project.getChosenType(r)) {
                        tracksManager.createAndAddProcessor(*description, &project.getUndoManager());
                    }
                }));
            }
        }
    }

private:
    static const int PROCESSOR_EDITOR_HEIGHT = 160;

    TextButton addProcessorButton{"Add Processor"};

    Project &project;
    TracksStateManager &tracksManager;
    ViewStateManager &viewManager;
    ProcessorGraph &audioGraphBuilder;

    Viewport contextPaneViewport, processorEditorsViewport;
    ContextPane contextPane;
    OwnedArray<ProcessorEditor> processorEditors;
    Component processorEditorsComponent;
    DrawableRectangle unfocusOverlay;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void refreshProcessors(const ValueTree& singleProcessorToRefresh={}) {
        const ValueTree &selectedTrack = tracksManager.getSelectedTrack();
        if (!selectedTrack.isValid()) {
            for (auto* editor : processorEditors) {
                editor->setVisible(false);
                editor->setProcessor(nullptr);
            }
        } else if (singleProcessorToRefresh.isValid()) {
            assignProcessorToEditor(singleProcessorToRefresh);
        } else {
            for (int processorSlot = 0; processorSlot < processorEditors.size(); processorSlot++) {
                const auto &processor = TracksStateManager::findProcessorAtSlot(selectedTrack, processorSlot);
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
                processorEditor->setSelected(tracksManager.isProcessorSelected(processor));
            }
        } else {
            processorEditor->setVisible(false);
            processorEditor->setProcessor(nullptr);
        }
    }

    void processorCreated(const ValueTree& processor) override {
        assignProcessorToEditor(processor);
        resized();
    };

    void processorHasBeenDestroyed(const ValueTree& processor) override {
        refreshProcessors();
    };

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::TRACK)) {
            refreshProcessors();
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::selected || i == IDs::selectedSlotsMask) {
            addProcessorButton.setVisible(tracksManager.getSelectedTrack().isValid());
            refreshProcessors();
        } else if (i == IDs::numProcessorSlots || i == IDs::numMasterProcessorSlots) {
            int numProcessorSlots = jmax(int(tree[IDs::numProcessorSlots]), int(tree[IDs::numMasterProcessorSlots]));
            while (processorEditors.size() < numProcessorSlots) {
                auto* processorEditor = new ProcessorEditor();
                processorEditorsComponent.addChildComponent(processorEditor);
                processorEditors.add(processorEditor);
            }
            processorEditors.removeLast(processorEditors.size() - numProcessorSlots);
        } else if (i == IDs::processorSlot) {
            refreshProcessors();
        } else if (i == IDs::focusedPane) {
            unfocusOverlay.setVisible(viewManager.isGridPaneFocused());
            unfocusOverlay.toFront(false);
        } else if (i == IDs::processorInitialized) {
            refreshProcessors();
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionEditor)
};
