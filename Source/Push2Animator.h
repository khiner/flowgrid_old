#pragma once

#include <view/InstrumentViewComponent.h>
#include "push2/Push2DisplayBridge.h"
#include "AudioGraphBuilder.h"

class Push2Animator : public Timer, public Component {
public:
    explicit Push2Animator(AudioGraphBuilder &audioGraphBuilder) {
        setSize(Push2Display::WIDTH, Push2Display::HEIGHT);
        instrumentViewComponent.setInstrument(audioGraphBuilder.getMainAudioProcessor()->getCurrentInstrument());
        addAndMakeVisible(instrumentViewComponent);
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
    InstrumentViewComponent instrumentViewComponent;
    Push2DisplayBridge displayBridge;
};
