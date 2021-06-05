#include "SelectionEditor.h"

#include "view/CustomColourIds.h"

SelectionEditor::SelectionEditor(Project &project, View &view, Tracks &tracks, StatefulAudioProcessorWrappers &processorWrappers)
        : project(project), view(view), tracks(tracks), pluginManager(project.getPluginManager()),
          processorWrappers(processorWrappers), contextPane(tracks, view) {
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
        project.selectProcessor(processorEditor->getProcessor());
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
    const auto *focusedTrack = tracks.getFocusedTrack();
    processorEditorsComponent.setBounds(0, 0, r.getWidth(), PROCESSOR_EDITOR_HEIGHT * (focusedTrack == nullptr ? 0 : focusedTrack->getNumProcessors()));
    r = processorEditorsComponent.getBounds();
    for (auto *editor : processorEditors) {
        if (editor->isVisible())
            editor->setBounds(r.removeFromTop(PROCESSOR_EDITOR_HEIGHT).reduced(4));
    }
}

void SelectionEditor::buttonClicked(Button *b) {
    if (b == &addProcessorButton) {
        const auto *focusedTrack = tracks.getFocusedTrack();
        if (focusedTrack != nullptr) {
            addProcessorMenu = std::make_unique<PopupMenu>();
            pluginManager.addPluginsToMenu(*addProcessorMenu);
            addProcessorMenu->showMenuAsync({}, ModalCallbackFunction::create([this](int r) {
                const auto &description = pluginManager.getChosenType(r);
                if (!description.name.isEmpty()) {
                    project.createProcessor(description);
                }
            }));
        }
    }
}

void SelectionEditor::refreshProcessors(const Processor *singleProcessorToRefresh, bool onlyUpdateFocusState) {
    const auto *focusedTrack = tracks.getFocusedTrack();
    if (focusedTrack == nullptr) {
        for (auto *editor : processorEditors) {
            editor->setVisible(false);
            editor->setProcessor(nullptr);
        }
    } else if (singleProcessorToRefresh != nullptr) {
        assignProcessorToEditor(singleProcessorToRefresh);
    } else {
        for (int processorSlot = 0; processorSlot < processorEditors.size(); processorSlot++) {
            const auto *processor = focusedTrack->getProcessorAtSlot(processorSlot);
            assignProcessorToEditor(processor, processorSlot, onlyUpdateFocusState);
        }
    }
    resized();
}

void SelectionEditor::assignProcessorToEditor(const Processor *processor, int processorSlot, bool onlyUpdateFocusState) const {
    auto *processorEditor = processorEditors.getUnchecked(processorSlot != -1 ? processorSlot : processor != nullptr ? processor->getSlot() : 0);
    if (processor != nullptr) {
        if (auto *processorWrapper = processorWrappers.getProcessorWrapperForProcessor(processor)) {
            if (!onlyUpdateFocusState) {
                processorEditor->setProcessor(processorWrapper);
                processorEditor->setVisible(true);
            }
            processorEditor->setSelected(tracks.getTrackForProcessor(processor)->isProcessorSelected(processor));
            processorEditor->setFocused(tracks.isProcessorFocused(processor));
        }
    } else {
        processorEditor->setVisible(false);
        processorEditor->setProcessor(nullptr);
    }
}

void SelectionEditor::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ViewIDs::focusedTrackIndex) {
        addProcessorButton.setVisible(tracks.getFocusedTrack() != nullptr);
        refreshProcessors();
    } else if (i == ProcessorIDs::initialized) {
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
    } else if (i == ProcessorIDs::slot) {
        refreshProcessors();
    } else if (i == ViewIDs::focusedPane) {
        unfocusOverlay.setVisible(view.isGridPaneFocused());
        unfocusOverlay.toFront(false);
    }
}
