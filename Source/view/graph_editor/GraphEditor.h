#pragma once

#include "ProcessorGraph.h"
#include "state/Project.h"
#include "GraphEditorPanel.h"
#include "view/SelectionEditor.h"
#include "TooltipBar.h"

class GraphEditor : public Component {
public:
    GraphEditor(ViewState &view, TracksState &tracks, ConnectionsState &connections, InputState &input, OutputState &output, ProcessorGraph &processorGraph, Project &project, PluginManager &pluginManager)
            : graphEditorPanel(view, tracks, connections, input, output, processorGraph, project, pluginManager),
              selectionEditor(project, view, tracks, processorGraph) {
        addAndMakeVisible(graphEditorPanel);
        addAndMakeVisible(selectionEditor);
    }

    void resized() override {
        auto r = getLocalBounds();
        graphEditorPanel.setBounds(r.removeFromLeft(int(r.getWidth() * 0.6f)));
        selectionEditor.setBounds(r);
    }

    bool closeAnyOpenPluginWindows() {
        return graphEditorPanel.closeAnyOpenPluginWindows();
    }

private:
    GraphEditorPanel graphEditorPanel;
    SelectionEditor selectionEditor;
};
