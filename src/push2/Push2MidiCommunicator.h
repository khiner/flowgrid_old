#pragma once

#include "midi/MidiCommunicator.h"
#include "view/push2/Push2Listener.h"
#include "view/push2/Push2Colours.h"

class Push2MidiCommunicator : public MidiCommunicator, private Push2Colours::Listener, private Timer {
public:
    static const uint8
            topKnob1 = 14, topKnob2 = 15, topKnob3 = 71, topKnob4 = 72, topKnob5 = 73, topKnob6 = 74, topKnob7 = 75,
            topKnob8 = 76, topKnob9 = 77, topKnob10 = 78, masterKnob = 79, tapTempo = 3, metronome = 9, setup = 30, user = 59,
            topDisplayButton1 = 102, topDisplayButton2 = 103, topDisplayButton3 = 104, topDisplayButton4 = 105,
            topDisplayButton5 = 106, topDisplayButton6 = 107, topDisplayButton7 = 108, topDisplayButton8 = 109,
            bottomDisplayButton1 = 20, bottomDisplayButton2 = 21, bottomDisplayButton3 = 22, bottomDisplayButton4 = 23,
            bottomDisplayButton5 = 24, bottomDisplayButton6 = 25, bottomDisplayButton7 = 26, bottomDisplayButton8 = 27,
            delete_ = 118, undo = 119, addDevice = 52, addTrack = 53, device = 110, mix = 112, browse = 111, clip = 113,
            mute = 60, solo = 61, stopClip = 29, master = 28, convert = 35, doubleLoop = 117, quantize = 116, duplicate = 88,
            new_ = 87, fixedLength = 90, automate = 89, record = 86, play = 85,
            divide_1_32t = 43, divide_1_32 = 42, divide_1_16_t = 41, divide_1_16 = 40,
            divide_1_8_t = 39, divide_1_8 = 38, divide_1_4t = 37, divide_1_4 = 36,
            left = 44, right = 45, up = 46, down = 47, repeat = 56, accent = 57, scale = 58, layout = 31, note = 50,
            session = 51, octaveUp = 55, octaveDown = 54, pageLeft = 62, pageRight = 63, shift = 49, select = 48;

    static const int upArrowDirection = 0, downArrowDirection = 1, leftArrowDirection = 2, rightArrowDirection = 3;

    static const int lowestPadNoteNumber = 36, highestPadNoteNumber = 99;

    explicit Push2MidiCommunicator(View &view, Push2Colours &push2Colours);

    ~Push2MidiCommunicator() override;

    void initialize() override;

    Push2Colours &getPush2Colours() { return push2Colours; }

    void setPush2Listener(Push2Listener *push2Listener) { this->push2Listener = push2Listener; }

    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override;

    void handleButtonPressMidiCcNumber(int ccNumber);

    void handleButtonReleaseMidiCcNumber(int ccNumber);

    void setAboveScreenButtonColour(int buttonIndex, const Colour &colour);

    void setBelowScreenButtonColour(int buttonIndex, const Colour &colour);

    void setAboveScreenButtonEnabled(int buttonIndex, bool enabled);

    void setBelowScreenButtonEnabled(int buttonIndex, bool enabled);

    void enableWhiteLedButton(int buttonCcNumber) const;

    void disableWhiteLedButton(int buttonCcNumber) const;

    void activateWhiteLedButton(int buttonCcNumber) const;

    void setColourButtonEnabled(int buttonCcNumber, bool enabled);

    void setButtonColour(int buttonCcNumber, const Colour &colour);

    void disablePad(int noteNumber);

    void setPadColour(int noteNumber, const Colour &colour);

    static uint8 ccNumberForArrowButton(int direction);

private:
    static constexpr int NO_ANIMATION_LED_CHANNEL = 1;
    static constexpr int BUTTON_HOLD_REPEAT_HZ = 10; // how often to repeat a repeatable button press when it is held
    static constexpr int BUTTON_HOLD_WAIT_FOR_REPEAT_MS = 500; // how long to wait before starting held button message repeats

    View &view;
    Push2Colours &push2Colours;
    Push2Listener *push2Listener{};

    int currentlyHeldRepeatableButtonCcNumber{0};

    bool holdRepeatIsHappeningNow{false};

    void sendMessageChecked(const MidiMessage &message) const;

    void registerAllIndexedColours();

    void colourAdded(const Colour &colour, uint8 colourIndex) override;

    void trackColourChanged(const String &trackUuid, const Colour &colour) override {}

    void buttonHoldStopped();

    void timerCallback() override;
};
