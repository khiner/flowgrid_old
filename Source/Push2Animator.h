#pragma once

#include <view/Push2ProcessorViewComponent.h>
#include "push2/Push2DisplayBridge.h"
#include "AudioGraphBuilder.h"

class Push2Animator : public Timer, public Component {
public:
    explicit Push2Animator(AudioGraphBuilder &audioGraphBuilder) {
        setSize(Push2Display::WIDTH, Push2Display::HEIGHT);
        StatefulAudioProcessor *currentAudioProcessor = audioGraphBuilder.getMainAudioProcessor()->getCurrentAudioProcessor();
        if (currentAudioProcessor != nullptr) {
            audioProcessorViewComponent.setStatefulAudioProcessor(currentAudioProcessor);
        }
        addAndMakeVisible(audioProcessorViewComponent);
        startTimer(60);
    }

private:
    /*!
     *  Render a frame and send it to the Push 2 display
     */
    void drawFrame();

    /*!
     *  the juce timer callback
     *  @see juce::Timer
     */
    void timerCallback() override;
private:
    Push2ProcessorViewComponent audioProcessorViewComponent;
    Push2DisplayBridge displayBridge;
};
