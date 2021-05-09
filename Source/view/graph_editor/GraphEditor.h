#pragma once

#include "ProcessorGraph.h"
#include "state/Project.h"
#include "GraphEditorPanel.h"
#include "view/SelectionEditor.h"
#include "TooltipBar.h"

class GraphEditor : public Component {
public:
    GraphEditor(ProcessorGraph &graph, Project &project)
            : graphPanel(graph, project),
              selectionEditor(project, graph) {
        addAndMakeVisible(graphPanel);
        addAndMakeVisible(selectionEditor);
    }

    void resized() override {
        auto r = getLocalBounds();
        graphPanel.setBounds(r.removeFromLeft(int(r.getWidth() * 0.6f)));
        selectionEditor.setBounds(r);
    }

    bool closeAnyOpenPluginWindows() {
        return graphPanel.closeAnyOpenPluginWindows();
    }

private:
    GraphEditorPanel graphPanel;
    SelectionEditor selectionEditor;
};
