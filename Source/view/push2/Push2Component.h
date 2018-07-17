#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorIds.h>
#include "push2/Push2DisplayBridge.h"
#include "ProcessorGraph.h"
#include "Push2ProcessorView.h"
#include "Push2ProcessorSelector.h"

class Push2Component :
        public Timer,
        public Component,
        private ProjectChangeListener {
public:
    explicit Push2Component(Project &project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder), processorSelector() {
        startTimer(60);

        addChildComponent(processorView);
        addChildComponent(processorSelector);

        project.addChangeListener(this);
        setSize(Push2Display::WIDTH, Push2Display::HEIGHT);
        processorView.setBounds(getBounds());
        processorSelector.setBounds(getBounds());
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
            const String &processorId = processorSelector.selectProcessor(project.getSelectedTrack(), buttonIndex);
            if (!processorId.isEmpty()) {
                // TODO get full plugin descriptor by using KnownPluginList
                //project.createAndAddProcessor(processorId);
            }
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

    Project &project;
    ProcessorGraph &audioGraphBuilder;

    Push2ProcessorView processorView;
    Push2ProcessorSelector processorSelector;
};
