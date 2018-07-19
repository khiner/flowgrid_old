#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorIds.h>
#include <push2/Push2MidiCommunicator.h>
#include "push2/Push2DisplayBridge.h"
#include "ProcessorGraph.h"
#include "Push2ProcessorView.h"
#include "Push2ProcessorSelector.h"
#include "Push2ComponentBase.h"

class Push2Component :
        public Timer,
        public Push2ComponentBase,
        private ProjectChangeListener {
public:
    explicit Push2Component(Project &project, Push2MidiCommunicator &push2MidiCommunicator, ProcessorGraph &audioGraphBuilder)
            : Push2ComponentBase(project, push2MidiCommunicator), audioGraphBuilder(audioGraphBuilder),
              processorView(project, push2MidiCommunicator), processorSelector(project, push2MidiCommunicator) {

        startTimer(60);

        addChildComponent(processorView);
        addChildComponent(processorSelector);

        project.addChangeListener(this);
        setBounds(0, 0, Push2Display::WIDTH, Push2Display::HEIGHT);
        processorView.setBounds(getLocalBounds());
        processorSelector.setBounds(getLocalBounds());
    }

    ~Push2Component() override {
        project.removeChangeListener(this);
    }

    void openProcessorSelector() {
        for (auto *c : getChildren())
            c->setVisible(false);
        processorSelector.setVisible(true);
    }

    void aboveScreenButtonPressed(int buttonIndex) {
        if (processorSelector.isVisible()) {
            if (const auto* selectedProcessor = processorSelector.selectTopProcessor(project.getSelectedTrack(), buttonIndex)) {
                project.createAndAddProcessor(*selectedProcessor);
            }
        }
    }

    void belowScreenButtonPressed(int buttonIndex) {
        if (processorSelector.isVisible()) {
            if (const auto* selectedProcessor = processorSelector.selectBottomProcessor(project.getSelectedTrack(), buttonIndex)) {
                project.createAndAddProcessor(*selectedProcessor);
            }
        }
    }

    void arrowPressed(Direction direction) {
        if (processorSelector.isVisible()) {
            processorSelector.arrowPressed(direction);
            return;
        }
        switch (direction) {
            case Direction::up: return project.moveSelectionUp();
            case Direction::down: return project.moveSelectionDown();
            case Direction::left: return project.moveSelectionLeft();
            case Direction::right: return project.moveSelectionRight();
            default: return;
        }
    }

private:
    /*!
     *  Render a frame and send it to the Push 2 display
     */
    void inline drawFrame() {
        static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

        auto &g = displayBridge.getGraphics();
        g.fillAll(CLEAR_COLOR);
        paintEntireComponent(g, true);
        displayBridge.writeFrameToDisplay();
    }

    void timerCallback() override {
        //auto t1 = std::chrono::high_resolution_clock::now();
        drawFrame();
        //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1).count() << '\n';
    }

    void itemSelected(const ValueTree& item) override {
        for (auto *c : getChildren())
            c->setVisible(false);

        if (!item.isValid()) {
        } else if (item.hasType(IDs::PROCESSOR)) {
            auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(item);
            if (processorWrapper != nullptr) {
                processorView.setProcessor(processorWrapper);
                processorView.setVisible(true);
            }
        }
//        if (project.getSelectedTrack().isValid()) {
//            processorSelector.setProcessorIds(getAvailableProcessorIdsForTrack(project.getSelectedTrack()));
//        }
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedProcessor()) {
            itemSelected(ValueTree());
        }
    }

private:
    Push2DisplayBridge displayBridge;

    ProcessorGraph &audioGraphBuilder;

    Push2ProcessorView processorView;
    Push2ProcessorSelector processorSelector;
};
