#pragma once

#include <processors/StatefulAudioProcessor.h>
#include "push2/Push2DisplayBridge.h"
#include "AudioGraphBuilder.h"

class Push2Component : public Timer, public Component, private ProjectChangeListener {
public:
    explicit Push2Component(Project &project, AudioGraphBuilder &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder) {
        setSize(Push2Display::WIDTH, Push2Display::HEIGHT);
        startTimer(60);

        for (int paramIndex = 0; paramIndex < MAX_PROCESSOR_PARAMS_TO_DISPLAY; paramIndex++) {
            Slider *slider = new Slider("Param " + String(paramIndex) + ": ");
            addAndMakeVisible(slider);

            labels.add(new Label(String(), slider->getName()))->attachToComponent(slider, false);
            sliders.add(slider);
        }

        project.addChangeListener(this);
    }

    ~Push2Component() override {
        project.removeChangeListener(this);
    }

private:
    const static int MAX_PROCESSOR_PARAMS_TO_DISPLAY = 8;

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

    void itemSelected(ValueTree item) override {
        for (auto *c : getChildren())
            c->setVisible(false);

        if (!item.isValid()) {
        } else if (item.hasType(IDs::PROCESSOR)) {
            StatefulAudioProcessor *processor = audioGraphBuilder.getAudioProcessor(project.getSelectedProcessor());
            if (processor != nullptr) {
                for (int i = 0; i < processor->getNumParameters(); i++) {
                    Slider *slider = sliders.getUnchecked(i);
                    Label *label = labels.getUnchecked(i);

                    slider->setVisible(true);
                    slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
                    slider->setBounds(i * Push2Display::WIDTH / 8, 20, Push2Display::WIDTH / 8, Push2Display::WIDTH / 8);
                    slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, Push2Display::HEIGHT, Push2Display::HEIGHT / 5);
                    label->setJustificationType(Justification::centred);

                    processor->getParameterInfo(i)->attachSlider(slider, label);
                }
            }
        }
    }

    void itemRemoved(ValueTree item) override {
        if (item == project.getSelectedProcessor()) {
            itemSelected(ValueTree());
        }
    }

private:
    Project &project;
    AudioGraphBuilder &audioGraphBuilder;

    OwnedArray<Slider> sliders;
    OwnedArray<Label> labels;
    Push2DisplayBridge displayBridge;
};
