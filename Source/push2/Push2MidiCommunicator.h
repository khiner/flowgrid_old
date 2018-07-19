#pragma once

#include <midi/MidiCommunicator.h>
#include <unordered_map>

class Push2MidiCommunicator : public MidiCommunicator {
public:
    Push2MidiCommunicator(): Push2MidiCommunicator("ableton push 2") {};

    explicit Push2MidiCommunicator(const std::string &deviceName) : MidiCommunicator(deviceName) {
        /*
         * https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc#Aftertouch
         * In channel pressure mode (default), the pad with the highest pressure determines the value sent. The pressure range that produces aftertouch is given by the aftertouch threshold pad parameters. The value curve is linear to the pressure and in range 0 to 127. See Pad Parameters.
         * In polyphonic key pressure mode, aftertouch for each pressed key is sent individually. The value is defined by the pad velocity curve and in range 1…​127. See Velocity Curve.
         * */
        static const uint8 setAftertouchModePolyphonic[] { 0x00, 0x21, 0x1D, 0x01, 0x01, 0x1E, 0x01 };
        auto polyphonicAftertouchSysExMessage = MidiMessage::createSysExMessage(setAftertouchModePolyphonic, 7);
        sendMessageChecked(polyphonicAftertouchSysExMessage);
    }

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

    enum class Direction { up, down, left, right, NA};

    // From https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc#Encoders:
    //
    // The value 0xxxxxx or 1yyyyyy gives the amount of accumulated movement since the last message. The faster you move, the higher the value.
    // The value is given as a 7 bit relative value encoded in two’s complement. 0xxxxxx indicates a movement to the right,
    // with decimal values from 1 to 63 (in practice, values above 20 are unlikely). 1yyyyyy means movement to the left, with decimal values from 127 to 64.
    // The total step count sent for a 360° turn is approx. 210, except for the detented tempo encoder, where one turn is 18 steps.
    //
    // This function returns a value between -1 and 1 normalized so that (roughly) the magnitude of a full rotation would sum to 1.
    // i.e. Turning 210 'steps' to the left would total to ~-1, and turning 210 steps to the right would total ~1.
    static float encoderCcMessageToRotationChange(const MidiMessage& message) {
        int value = message.getControllerValue();
        if (value <= 63) {
            return static_cast<float>(value) / 210.0f;
        } else {
            return (static_cast<float>(value) - 128.0f) / 210.0f;
        }
    }

    static uint8 ccNumberForTopKnobIndex(int topKnobIndex) {
        switch (topKnobIndex) {
            case 0: return topKnob3;
            case 1: return topKnob4;
            case 2: return topKnob5;
            case 3: return topKnob6;
            case 4: return topKnob7;
            case 5: return topKnob8;
            case 6: return topKnob9;
            case 7: return topKnob10;
            default: return 0;
        }
    }

    static uint8 ccNumberForArrowButton(Direction direction) {
        switch(direction) {
            case Direction::up: return up;
            case Direction::down: return down;
            case Direction::left: return left;
            case Direction::right: return right;
            default: return 0;
        }
    }

    static Direction directionForArrowButtonCcNumber(int ccNumber) {
        switch (ccNumber) {
            case left: return Direction::left;
            case right: return Direction::right;
            case up: return Direction::up;
            case down: return Direction::down;
            default: return Direction::NA;
        }
    }

    static bool isEncoderCcNumber(int ccNumber) {
        return ccNumber == topKnob1 || ccNumber == topKnob2 || ccNumber == masterKnob || isAboveScreenEncoderCcNumber(ccNumber);
    }

    static bool isAboveScreenEncoderCcNumber(int ccNumber) {
        return ccNumber >= topKnob3 && ccNumber <= topKnob10;
    }

    static bool isArrowButtonCcNumber(int ccNumber) {
        return ccNumber >= left && ccNumber <= down;
    }

    static bool isAboveScreenButtonCcNumber(int ccNumber) {
        return ccNumber >= topDisplayButton1 && ccNumber <= topDisplayButton8;
    }

    static bool isBelowScreenButtonCcNumber(int ccNumber) {
        return ccNumber >= bottomDisplayButton1 && ccNumber <= bottomDisplayButton8;
    }

    static bool isButtonPressControlMessage(const MidiMessage &message) {
        return message.getControllerValue() == 127;
    }

    static bool isButtonReleaseControlMessage(const MidiMessage &message) {
        return message.getControllerValue() == 0;
    }

    void setAboveScreenButtonColour(int buttonIndex, const Colour &colour) {
        setButtonColour(buttonIndex + topDisplayButton1, colour);
    }

    void setBelowScreenButtonColour(int buttonIndex, const Colour &colour) {
        setButtonColour(buttonIndex + bottomDisplayButton1, colour);
    }

    void setAboveScreenButtonEnabled(int buttonIndex, bool enabled) {
        setColourButtonEnabled(buttonIndex + topDisplayButton1, enabled);
    }

    void setBelowScreenButtonEnabled(int buttonIndex, bool enabled) {
        setColourButtonEnabled(buttonIndex + bottomDisplayButton1, enabled);
    }

    void setArrowButtonEnabled(Direction direction, bool enabled) const {
        uint8 ccNumber = ccNumberForArrowButton(direction);
        sendMessageChecked(MidiMessage::controllerEvent(NO_ANIMATION_LED_CHANNEL, ccNumber, enabled ? CHAR_MAX : 0));
    }

    void setAllArrowButtonsEnabled(bool enabled) const {
        setArrowButtonEnabled(Direction::left, enabled);
        setArrowButtonEnabled(Direction::right, enabled);
        setArrowButtonEnabled(Direction::up, enabled);
        setArrowButtonEnabled(Direction::down, enabled);
    }

private:
    static const int NO_ANIMATION_LED_CHANNEL = 1;

    std::unordered_map<String, int> indexForColour;
    Array<Colour> colours;

    void sendMessageChecked(const MidiMessage& message) const {
        if (isOutputConnected()) {
            midiOutput->sendMessageNow(message);
        }
    }

    void setButtonColour(int buttonCcNumber, const Colour &colour) {
        auto entry = indexForColour.find(colour.toString());
        if (entry == indexForColour.end()) {
            addColour(colour);
            entry = indexForColour.find(colour.toString());
        }

        jassert(entry != indexForColour.end());
        sendMessageChecked(MidiMessage::controllerEvent(NO_ANIMATION_LED_CHANNEL, buttonCcNumber, entry->second));
    }

    void setColourButtonEnabled(int buttonCcNumber, bool enabled) {
        if (enabled) {
            setButtonColour(buttonCcNumber, Colours::white);
        } else {
            sendMessageChecked(MidiMessage::controllerEvent(NO_ANIMATION_LED_CHANNEL, buttonCcNumber, 0));
        }
    }

    void addColour(const Colour &colour) {
        jassert(colours.size() < CHAR_MAX); // no sensible way to prioritize which colours to switch out

        auto colourIndex = static_cast<uint8>(colours.size() + 1);
        uint32 argb = colour.getARGB();
        // 8 bytes: 2 for each of R, G, B, W. First byte contains the 7 LSBs; Second byte contains the 1 MSB.
        uint8 bgra[8];

        for (int i = 0; i < 4; i++) {
            auto c = static_cast<uint8>(argb >> (i * CHAR_BIT));
            bgra[i * 2] = static_cast<uint8>(c & 0x7F);
            bgra[i * 2 + 1] = static_cast<uint8>(c & 0x80) >> 7;
        }

        const uint8 setLedColourPaletteEntryCommand[] { 0x00, 0x21, 0x1D, 0x01, 0x01, 0x03, colourIndex,
                                                        bgra[4], bgra[5], bgra[2], bgra[3], bgra[0], bgra[1], bgra[6], bgra[7] };
        auto setLedColourPaletteEntryMessage = MidiMessage::createSysExMessage(setLedColourPaletteEntryCommand, 15);
        sendMessageChecked(setLedColourPaletteEntryMessage);
        static const uint8 reapplyColorPaletteCommand[] { 0x00, 0x21, 0x1D, 0x01, 0x01, 0x05 };
        sendMessageChecked(MidiMessage::createSysExMessage(reapplyColorPaletteCommand, 6));

        colours.add(colour);
        indexForColour.insert(std::make_pair(colour.toString(), colourIndex));
    }
};
