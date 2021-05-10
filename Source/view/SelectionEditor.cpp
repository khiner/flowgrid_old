#include "SelectionEditor.h"

#include <state/Identifiers.h>
#include "view/CustomColourIds.h"

SelectionEditor::SelectionEditor(Project &project, ProcessorGraph &audioGraphBuilder)
        : project(project), tracks(project.getTracks()), view(project.getView()),
          pluginManager(project.getPluginManager()), audioGraphBuilder(audioGraphBuilder), contextPane(project) {
    tracks.addListener(this);
    view.addListener(this);

    addAndMakeVisible(addProcessorButton);
    addAndMakeVisible(processorEditorsViewport);
    addAndMakeVisible(contextPaneViewport);
    addAndMakeVisible(statusBar);
    unfocusOverlay.setFill(findColour(CustomColourIds::unfocusedOverlayColourId));
    addChildComponent(unfocusOverlay);

    addProcessorButton.addListener(this);
    processorEditorsViewport.setViewedComponent(&processorEditorsComponent, false);
    processorEditorsViewport.setScrollBarsShown(true, false);
    contextPaneViewport.setViewedComponent(&contextPane, false);
    addMouseListener(this, true);
}

SelectionEditor::~SelectionEditor() {
    removeMouseListener(this);
    tracks.removeListener(this);
    view.removeListener(this);
}

void SelectionEditor::mouseDown(const MouseEvent &event) {
    view.focusOnEditorPane();
    if (auto *processorEditor = dynamic_cast<ProcessorEditor *>(event.originalComponent)) {
        project.selectProcessor(processorEditor->getProcessorState());
    }
}


void SelectionEditor::paint(Graphics &g) {
    g.setColour(findColour(TextEditor::backgroundColourId));
    g.drawLine(1, 0, 1, getHeight(), 2);
    g.drawLine(0, contextPaneViewport.getY(), getWidth(), contextPaneViewport.getY(), 2);
}

void SelectionEditor::resized() {
    auto r = getLocalBounds().reduced(4);
    statusBar.setBounds(r.removeFromBottom(20));
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
    for (auto *editor : processorEditors) {
        if (editor->isVisible())
            editor->setBounds(r.removeFromTop(PROCESSOR_EDITOR_HEIGHT).reduced(4));
    }
}

void SelectionEditor::buttonClicked(Button *b) {
    if (b == &addProcessorButton) {
        const auto &focusedTrack = tracks.getFocusedTrack();
        if (focusedTrack.isValid()) {
            addProcessorMenu = std::make_unique<PopupMenu>();
            pluginManager.addPluginsToMenu(*addProcessorMenu, focusedTrack);
            addProcessorMenu->showMenuAsync({}, ModalCallbackFunction::create([this](int r) {
                const auto &description = pluginManager.getChosenType(r);
                if (!description.name.isEmpty()) {
                    project.createProcessor(description);
                }
            }));
        }
    }
}

void SelectionEditor::refreshProcessors(const ValueTree &singleProcessorToRefresh, bool onlyUpdateFocusState) {
    const ValueTree &focusedTrack = tracks.getFocusedTrack();
    if (!focusedTrack.isValid()) {
        for (auto *editor : processorEditors) {
            editor->setVisible(false);
            editor->setProcessor(nullptr);
        }
    } else if (singleProcessorToRefresh.isValid()) {
        assignProcessorToEditor(singleProcessorToRefresh);
    } else {
        for (int processorSlot = 0; processorSlot < processorEditors.size(); processorSlot++) {
            const auto &processor = TracksState::getProcessorAtSlot(focusedTrack, processorSlot);
            assignProcessorToEditor(processor, processorSlot, onlyUpdateFocusState);
        }
    }
    resized();
}

void SelectionEditor::assignProcessorToEditor(const ValueTree &processor, int processorSlot, bool onlyUpdateFocusState) const {
    auto *processorEditor = processorEditors.getUnchecked(processorSlot != -1 ? processorSlot : int(processor[IDs::processorSlot]));
    if (processor.isValid()) {
        if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(processor)) {
            if (!onlyUpdateFocusState) {
                processorEditor->setProcessor(processorWrapper);
                processorEditor->setVisible(true);
            }
            processorEditor->setSelected(TracksState::isProcessorSelected(processor));
            processorEditor->setFocused(tracks.isProcessorFocused(processor));
        }
    } else {
        processorEditor->setVisible(false);
        processorEditor->setProcessor(nullptr);
    }
}

void SelectionEditor::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (child.hasType(IDs::TRACK) || child.hasType(IDs::PROCESSOR)) {
        refreshProcessors();
    }
}

void SelectionEditor::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
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
            auto *processorEditor = new ProcessorEditor();
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
