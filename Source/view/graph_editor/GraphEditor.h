#pragma once

#include <ProcessorGraph.h>
#include <Project.h>
#include "JuceHeader.h"
#include "GraphEditorPanel.h"

class GraphEditor : public Component {
public:
    GraphEditor(ProcessorGraph &graph, Project &project)
            : graph(graph), project(project), graphPanel(graph, project) {
        addAndMakeVisible(graphPanel);
        addAndMakeVisible(statusBar);
        graphPanel.updateComponents();
    }

    void resized() override {
        auto r = getLocalBounds();
        statusBar.setBounds(r.removeFromBottom(20));
        graphPanel.setBounds(r);
    }

private:
    ProcessorGraph &graph;
    Project &project;

    GraphEditorPanel graphPanel;

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
