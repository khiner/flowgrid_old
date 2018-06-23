#pragma once

#include <push2/Push2MidiCommunicator.h>
#include <audio_processors/MainProcessor.h>
#include <view/Push2ViewComponent.h>
#include "push2/Push2DisplayBridge.h"

class Push2Animator : public Timer {
public:
    explicit Push2Animator(Push2ViewComponent* push2ViewComponent) {
        this->push2ViewComponent = push2ViewComponent;
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
    Push2DisplayBridge displayBridge;
    Push2ViewComponent* push2ViewComponent;
};
