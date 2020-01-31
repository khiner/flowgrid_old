#include <JuceHeader.h>
#include <Utilities.h>
#include <state/Identifiers.h>
#include <PluginManager.h>
#include "processor_editor/ProcessorEditor.h"
#include "view/context_pane/ContextPane.h"
#include "view/CustomColourIds.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        private Button::Listener,
                        private ValueTree::Listener {
public:
    SelectionEditor(Project& project, ProcessorGraph &audioGraphBuilder)
            : project(project), tracks(project.getTracksManager()),
              view(project.getViewStateManager()),
              audioGraphBuilder(audioGraphBuilder), contextPane(project) {
        this->project.getState().addListener(this);

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
        removeMouseListener(this);
        project.getState().removeListener(this);
    }

    void mouseDown(const MouseEvent& event) override {
        view.focusOnEditorPane();
        if (auto* processorEditor = dynamic_cast<ProcessorEditor*>(event.originalComponent)) {
            project.selectProcessor(processorEditor->getProcessorState());
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
        processorEditorsComponent.setBounds(0, 0, r.getWidth(), PROCESSOR_EDITOR_HEIGHT *
                tracks.getFocusedTrack().getNumChildren());
        r = processorEditorsComponent.getBounds();
        for (auto* editor : processorEditors) {
            if (editor->isVisible())
                editor->setBounds(r.removeFromTop(PROCESSOR_EDITOR_HEIGHT).reduced(4));
        }
    }

    void buttonClicked(Button *b) override {
        if (b == &addProcessorButton) {
            const auto& focusedTrack = tracks.getFocusedTrack();
            if (focusedTrack.isValid()) {
                addProcessorMenu = std::make_unique<PopupMenu>();
                project.addPluginsToMenu(*addProcessorMenu, focusedTrack);
                addProcessorMenu->showMenuAsync({}, ModalCallbackFunction::create([this](int r) {
                    if (auto *description = project.getChosenType(r)) {
                        project.createProcessor(*description);
                    }
                }));
            }
        }
    }

private:
    static const int PROCESSOR_EDITOR_HEIGHT = 160;

    TextButton addProcessorButton{"Add Processor"};

    Project &project;
    TracksState &tracks;
    ViewState &view;
    ProcessorGraph &audioGraphBuilder;

    Viewport contextPaneViewport, processorEditorsViewport;
    ContextPane contextPane;
    OwnedArray<ProcessorEditor> processorEditors;
    Component processorEditorsComponent;
    DrawableRectangle unfocusOverlay;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void refreshProcessors(const ValueTree& singleProcessorToRefresh={}, bool onlyUpdateFocusState=false) {
        const ValueTree &focusedTrack = tracks.getFocusedTrack();
        if (!focusedTrack.isValid()) {
            for (auto* editor : processorEditors) {
                editor->setVisible(false);
                editor->setProcessor(nullptr);
            }
        } else if (singleProcessorToRefresh.isValid()) {
            assignProcessorToEditor(singleProcessorToRefresh);
        } else {
            for (int processorSlot = 0; processorSlot < processorEditors.size(); processorSlot++) {
                const auto &processor = TracksState::findProcessorAtSlot(focusedTrack, processorSlot);
                assignProcessorToEditor(processor, processorSlot, onlyUpdateFocusState);
            }
        }
        resized();
    }

    void assignProcessorToEditor(const ValueTree &processor, int processorSlot=-1, bool onlyUpdateFocusState=false) const {
        auto* processorEditor = processorEditors.getUnchecked(processorSlot != -1 ? processorSlot : int(processor[IDs::processorSlot]));
        if (processor.isValid()) {
            if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(processor)) {
                if (!onlyUpdateFocusState) {
                    processorEditor->setProcessor(processorWrapper);
                    processorEditor->setVisible(true);
                }
                processorEditor->setSelected(tracks.isProcessorSelected(processor));
                processorEditor->setFocused(tracks.isProcessorFocused(processor));
            }
        } else {
            processorEditor->setVisible(false);
            processorEditor->setProcessor(nullptr);
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::TRACK) || child.hasType(IDs::PROCESSOR)) {
            refreshProcessors();
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::focusedTrackIndex) {
            addProcessorButton.setVisible(tracks.getFocusedTrack().isValid());
            refreshProcessors();
        } else if (i == IDs::processorInitialized) {
            refreshProcessors(); // TODO only the new processor
        } else if (i == IDs::focusedProcessorSlot) {
            refreshProcessors({}, true);
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
            unfocusOverlay.setVisible(view.isGridPaneFocused());
            unfocusOverlay.toFront(false);
        }
    }

    void valueTreeChildOrderChanged(ValueTree &tree, int, int) override {}

    void valueTreeParentChanged(ValueTree &) override {}

    void valueTreeRedirected(ValueTree &) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionEditor)
};
