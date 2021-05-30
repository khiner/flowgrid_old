#pragma once

#include "model/Project.h"
#include "PluginManager.h"
#include "processor_editor/ProcessorEditor.h"
#include "view/graph_editor/TooltipBar.h"
#include "view/context_pane/ContextPane.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        private Button::Listener,
                        private ValueTree::Listener,
                        private Tracks::Listener {
public:
    SelectionEditor(Project &project, View &view, Tracks &tracks, StatefulAudioProcessorWrappers &processorWrappers);

    ~SelectionEditor() override;

    void mouseDown(const MouseEvent &event) override;
    void paint(Graphics &g) override;
    void resized() override;
    void buttonClicked(Button *b) override;

private:
    static const int PROCESSOR_EDITOR_HEIGHT = 160;

    TextButton addProcessorButton{"Add Processor"};

    Project &project;
    View &view;
    Tracks &tracks;
    PluginManager &pluginManager;
    StatefulAudioProcessorWrappers &processorWrappers;

    Viewport contextPaneViewport, processorEditorsViewport;
    ContextPane contextPane;
    TooltipBar statusBar;

    OwnedArray<ProcessorEditor> processorEditors;
    Component processorEditorsComponent;
    DrawableRectangle unfocusOverlay;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void refreshProcessors(const ValueTree &singleProcessorToRefresh = {}, bool onlyUpdateFocusState = false);
    void assignProcessorToEditor(const ValueTree &processor, int processorSlot = -1, bool onlyUpdateFocusState = false) const;

    void trackAdded(Track *track) override { }
    void trackRemoved(Track *track, int oldIndex) override { refreshProcessors(); }
    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (Processor::isType(child)) refreshProcessors();
    }
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
