#pragma once

#include "push2/Push2DisplayBridge.h"

/*!
 *  A simple demo class that will draw some animation on the push display
 *  and listen to incoming midi input from push
 */

class Demo
        : public Timer, public MidiInputCallback {
public:
    /*!
     *  Initialises and starts the demo
     *
     *  also instantiates the result of the initialisation process
     */
    Demo();

    using midicb_t = std::function<void(const MidiMessage &)>;

    /*!
     *  Allows a client to hook a collback in order to process midi messages recieved from push2
     */
    void SetMidiInputCallback(const midicb_t &func);

private:

    /*!
     *  renders a frame and send it to the push display
     */
    void drawFrame();

    /*!
     *  look for the push 2 input device and starts listening to it
     *
     *  \return the result of the initialisation process
     */
    void openMidiDevice();

    /*!
     *  the juce midi incoming message callback
     *  @see juce::MidiInputCallback
     */
    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override;

    /*!
     *  the juce timer callback
     *  @see juce::Timer
     */
    void timerCallback() override;
private:
    ableton::Push2DisplayBridge bridge;    /*!< The bridge allowing to use juce::graphics for push */
    ableton::Push2Display push2Display;                  /*!< The low-level push2 class */
    std::unique_ptr<MidiInput> midiInput;  /*!< Push2's midi input */
    midicb_t midiCallback;                 /*!> The midi callback to call when incoming messages are recieved */
    float elapsed;                         /*!> Fake elapsed time used for the animation */
};
