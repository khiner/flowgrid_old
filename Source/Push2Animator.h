#pragma once

#include <audio_processors/StatefulAudioProcessor.h>
#include <drow/ValueTreeItems.h>
#include "push2/Push2DisplayBridge.h"
#include "AudioGraphBuilder.h"

class Push2Animator : public Timer, public Component, private ProjectChangeListener {
public:
    explicit Push2Animator(Project &project, AudioGraphBuilder &audioGraphBuilder) : audioGraphBuilder(audioGraphBuilder) {
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

    void itemSelected(ValueTreeItem *item) override {
        for (auto *c : getChildren())
            c->setVisible(false);

        if (item == nullptr) {
        } else if (auto *processor = dynamic_cast<Processor *> (item)) {
            StatefulAudioProcessor *selectedAudioProcessor = audioGraphBuilder.getAudioProcessorWithUuid(processor->getState().getProperty(IDs::uuid, processor->getUndoManager()));
            if (selectedAudioProcessor != nullptr) {
                sliderAttachments.clear();

                for (int sliderIndex = 0; sliderIndex < selectedAudioProcessor->getNumParameters(); sliderIndex++) {
                    Slider *slider = sliders[sliderIndex];
                    Label *label = labels[sliderIndex];

                    slider->setVisible(true);

                    const String &parameterId = selectedAudioProcessor->getParameterIdentifier(sliderIndex);
                    label->setText(selectedAudioProcessor->getState()->getParameter(parameterId)->name, dontSendNotification);
                    slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
                    slider->setBounds(sliderIndex * Push2Display::WIDTH / 8, 20, Push2Display::WIDTH / 8, Push2Display::WIDTH / 8);
                    slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, Push2Display::HEIGHT, Push2Display::HEIGHT / 5);
                    label->setJustificationType(Justification::centred);

                    auto sliderAttachment = new AudioProcessorValueTreeState::SliderAttachment(*selectedAudioProcessor->getState(), parameterId, *slider);
                    sliderAttachments.add(sliderAttachment);
                }
            }
        } else if (auto *clip = dynamic_cast<Clip *> (item)) {
        } else if (auto *track = dynamic_cast<Track *> (item)) {
        }
    }

private:
    AudioGraphBuilder &audioGraphBuilder;

    OwnedArray<Slider> sliders;
    OwnedArray<Label> labels;
    OwnedArray<AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    Push2DisplayBridge displayBridge;
};
