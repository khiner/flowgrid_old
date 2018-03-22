#pragma once

#include <push2/Push2MidiCommunicator.h>
#include <audio_processors/MainProcessor.h>
#include "push2/Push2DisplayBridge.h"

class Push2Animator : public Timer, public Component {
public:
    explicit Push2Animator(const std::vector<std::unique_ptr<Slider> >& sliders);

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

private:
};
