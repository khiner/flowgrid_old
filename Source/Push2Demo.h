#pragma once

#include "push2/Push2DisplayBridge.h"

/*!
 *  A simple demo class that will draw some animation on the push display
 *  and listen to incoming midi input from push
 */

class Demo : public Timer, public MidiInputCallback {
public:
    Demo();

    using midicb_t = std::function<void(const MidiMessage &)>;

    /*!
     *  Hook up a callback to process MIDI messages received from Push 2
     */
    void setMidiInputCallback(const midicb_t &func);

private:
    /*!
     *  Render a frame and send it to the Push 2 display
     */
    void drawFrame();

    /*!
     *  Look for the Push 2 input device and starts listening to it
     */
    void openMidiDevice();

    /*!
     *  The Juce MIDI incoming message callback
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
    std::unique_ptr<MidiInput> midiInput;  /*!< Push2's midi input */
    midicb_t midiCallback;                 /*!> The midi callback to call when incoming messages are recieved */
    float elapsed;                         /*!> Fake elapsed time used for the animation */
};
