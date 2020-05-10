#pragma once

#include <ProcessorGraph.h>
#include <state/Project.h>
#include "JuceHeader.h"
#include "GraphEditorPanel.h"
#include "view/SelectionEditor.h"

class GraphEditor : public Component {
public:
    GraphEditor(ProcessorGraph &graph, Project &project)
            : project(project),
              graphPanel(graph, project),
              selectionEditor(project, graph) {
        addAndMakeVisible(statusBar);
        addAndMakeVisible(graphPanel);
        addAndMakeVisible(selectionEditor);
    }

    void resized() override {
        auto r = getLocalBounds();
        statusBar.setBounds(r.removeFromBottom(20));
        graphPanel.setBounds(r.removeFromLeft(int(r.getWidth() * 0.6f)));
        selectionEditor.setBounds(r);
    }

    bool closeAnyOpenPluginWindows() {
        return graphPanel.closeAnyOpenPluginWindows();
    }

private:
    Project &project;
    GraphEditorPanel graphPanel;
    SelectionEditor selectionEditor;

    struct TooltipBar : public Component, private Timer {
        TooltipBar() {
            startTimer(100);
        }

        void paint(Graphics &g) override {
            g.setFont(Font(getHeight() * 0.7f, Font::bold));
            g.setColour(findColour(TextEditor::textColourId));
            g.drawFittedText(tip, 10, 0, getWidth() - 12, getHeight(), Justification::centredLeft, 1);
        }

        void timerCallback() override {
            String newTip;

            if (auto *underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
                if (auto *ttc = dynamic_cast<TooltipClient *> (underMouse))
                    if (!(underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
                        newTip = ttc->getTooltip();

            if (newTip != tip) {
                tip = newTip;
                repaint();
            }
        }

        String tip;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipBar)
    };

    TooltipBar statusBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditor)
};
