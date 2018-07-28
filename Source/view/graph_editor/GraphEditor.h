#pragma once

#include <ProcessorGraph.h>
#include <Project.h>
#include "JuceHeader.h"
#include "GraphEditorPanel.h"

class GraphEditor : public Component {
public:
    GraphEditor(ProcessorGraph &graph, Project &project)
            : graph(graph), project(project) {
        init();
    }

    ~GraphEditor() override {
        releaseGraph();
    }

    void resized() override {
        auto r = getLocalBounds();
        const int titleBarHeight = 40;
        const int keysHeight = 60;
        const int statusHeight = 20;

        statusBar->setBounds(r.removeFromBottom(statusHeight));
        graphPanel->setBounds(r);
    }

    void releaseGraph() {
        if (graphPanel != nullptr) {
            graphPanel = nullptr;
        }

        statusBar = nullptr;
    }

    std::unique_ptr<GraphEditorPanel> graphPanel;
private:
    ProcessorGraph &graph;
    Project &project;

    struct TooltipBar : public Component, private Timer {
        TooltipBar() {
            startTimer(100);
        }

        void paint(Graphics &g) override {
            g.setFont(Font(getHeight() * 0.7f, Font::bold));
            g.setColour(Colours::black);
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

    std::unique_ptr<TooltipBar> statusBar;

    void init() {
        graphPanel = std::make_unique<GraphEditorPanel>(graph, project);
        addAndMakeVisible(graphPanel.get());

        statusBar = std::make_unique<TooltipBar>();
        addAndMakeVisible(statusBar.get());

        graphPanel->updateComponents();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditor)
};
