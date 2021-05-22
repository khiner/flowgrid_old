#include "SelectionEditor.h"

#include "view/CustomColourIds.h"

SelectionEditor::SelectionEditor(Project &project, View &view, Tracks &tracks, ProcessorGraph &audioGraphBuilder)
        : project(project), view(view), tracks(tracks), pluginManager(project.getPluginManager()),
          audioGraphBuilder(audioGraphBuilder), contextPane(tracks, view) {
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
            const auto &processor = Tracks::getProcessorAtSlot(focusedTrack, processorSlot);
            assignProcessorToEditor(processor, processorSlot, onlyUpdateFocusState);
        }
    }
    resized();
}

void SelectionEditor::assignProcessorToEditor(const ValueTree &processor, int processorSlot, bool onlyUpdateFocusState) const {
    auto *processorEditor = processorEditors.getUnchecked(processorSlot != -1 ? processorSlot : Processor::getSlot(processor));
    if (processor.isValid()) {
        if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(processor)) {
            if (!onlyUpdateFocusState) {
                processorEditor->setProcessor(processorWrapper);
                processorEditor->setVisible(true);
            }
            processorEditor->setSelected(Tracks::isProcessorSelected(processor));
            processorEditor->setFocused(tracks.isProcessorFocused(processor));
        }
    } else {
        processorEditor->setVisible(false);
        processorEditor->setProcessor(nullptr);
    }
}

void SelectionEditor::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (Track::isType(child) || Processor::isType(child)) {
        refreshProcessors();
    }
}

void SelectionEditor::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ViewIDs::focusedTrackIndex) {
        addProcessorButton.setVisible(tracks.getFocusedTrack().isValid());
        refreshProcessors();
    } else if (i == ProcessorIDs::processorInitialized) {
        refreshProcessors(); // TODO only the new processor
    } else if (i == ViewIDs::focusedProcessorSlot) {
        refreshProcessors({}, true);
    } else if (i == ViewIDs::numProcessorSlots || i == ViewIDs::numMasterProcessorSlots) {
        int numProcessorSlots = jmax(view.getNumProcessorSlots(true), view.getNumProcessorSlots(false));
        while (processorEditors.size() < numProcessorSlots) {
            auto *processorEditor = new ProcessorEditor();
            processorEditorsComponent.addChildComponent(processorEditor);
            processorEditors.add(processorEditor);
        }
        processorEditors.removeLast(processorEditors.size() - numProcessorSlots);
    } else if (i == ProcessorIDs::processorSlot) {
        refreshProcessors();
    } else if (i == ViewIDs::focusedPane) {
        unfocusOverlay.setVisible(view.isGridPaneFocused());
        unfocusOverlay.toFront(false);
    }
}
