#pragma once

#include <audio_processors/StatefulAudioProcessor.h>
#include "push2/Push2DisplayBridge.h"
#include "AudioGraphBuilder.h"

class Push2Animator : public Timer, public Component {
public:
    explicit Push2Animator(AudioGraphBuilder &audioGraphBuilder) {
        setSize(Push2Display::WIDTH, Push2Display::HEIGHT);
        StatefulAudioProcessor *currentAudioProcessor = audioGraphBuilder.getMainAudioProcessor()->getSelectedAudioProcessor();
        if (currentAudioProcessor != nullptr) {
            setStatefulAudioProcessor(currentAudioProcessor);
        }
        startTimer(60);
    }

    void setStatefulAudioProcessor(StatefulAudioProcessor *statefulAudioProcessor) {
        removeAllChildren();

        sliderAttachments.clear();
        labels.clear();
        sliders.clear();

        for (int sliderIndex = 0; sliderIndex < statefulAudioProcessor->getNumParameters(); sliderIndex++) {
            auto slider = std::make_unique<Slider>();
            const String &parameterId = statefulAudioProcessor->getParameterIdentifier(sliderIndex);
            auto sliderAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(*(statefulAudioProcessor->getState()), parameterId, *slider);

            auto label = std::make_unique<Label>();
            label->setText(statefulAudioProcessor->getState()->getParameter(parameterId)->name, dontSendNotification);
            addAndMakeVisible(*slider);
            addAndMakeVisible(*label);
            slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
            slider->setBounds(sliderIndex * Push2Display::WIDTH / 8, 20, Push2Display::WIDTH / 8, Push2Display::WIDTH / 8);
            slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, Push2Display::HEIGHT, Push2Display::HEIGHT / 5);
            label->attachToComponent(slider.get(), false);
            label->setJustificationType(Justification::centred);
            labels.push_back(std::move(label));
            sliders.push_back(std::move(slider));
            sliderAttachments.push_back(std::move(sliderAttachment));
        }
    }
private:
    /*!
     *  Render a frame and send it to the Push 2 display
     */
    void drawFrame() {
        static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

        auto &g = displayBridge.getGraphics();
        g.fillAll(CLEAR_COLOR);
        paintEntireComponent(g, true);
        displayBridge.writeFrameToDisplay();
    }

    /*!
     *  the juce timer callback
     *  @see juce::Timer
     */
    void timerCallback() {
        //auto t1 = std::chrono::high_resolution_clock::now();
        drawFrame();
        //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1).count() << '\n';
    }

private:
    std::vector<std::unique_ptr<Slider> > sliders;
    std::vector<std::unique_ptr<Label> > labels;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> > sliderAttachments;
    Push2DisplayBridge displayBridge;
};
