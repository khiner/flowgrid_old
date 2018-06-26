#pragma once

#include <view/InstrumentViewComponent.h>
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
    InstrumentViewComponent audioProcessorViewComponent;
    Push2DisplayBridge displayBridge;
};
