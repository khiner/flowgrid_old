#pragma once

#include <midi/MidiCommunicator.h>
#include <unordered_map>

namespace SysExCommands {
    static const int MESSAGE_SIZE = 7;
    /*
     https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc#Aftertouch
     In channel pressure mode (default), the pad with the highest pressure determines the value sent. The pressure range that produces aftertouch is given by the aftertouch threshold pad parameters. The value curve is linear to the pressure and in range 0 to 127. See Pad Parameters.
     In polyphonic key pressure mode, aftertouch for each pressed key is sent individually. The value is defined by the pad velocity curve and in range 1…​127. See Velocity Curve.
     */
    static const unsigned char SET_AFTERTOUCH_MODE_POLYPHONIC[] { 0x00, 0x21, 0x1D, 0x01, 0x01, 0x1E, 0x01 };
}

class Push2MidiCommunicator : public MidiCommunicator {
public:
    Push2MidiCommunicator(): Push2MidiCommunicator("ableton push 2") {};

    explicit Push2MidiCommunicator(const std::string &deviceName) : MidiCommunicator(deviceName) {
        auto polyphonicAftertouchSysExMessage = MidiMessage::createSysExMessage(SysExCommands::SET_AFTERTOUCH_MODE_POLYPHONIC, SysExCommands::MESSAGE_SIZE);
        midiOutput->sendMessageNow(polyphonicAftertouchSysExMessage);
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

    static int ccNumberForTopKnobIndex(int topKnobIndex) {
        switch (topKnobIndex) {
            case 0: return topKnob3;
            case 1: return topKnob4;
            case 2: return topKnob5;
            case 3: return topKnob6;
            case 4: return topKnob7;
            case 5: return topKnob8;
            case 6: return topKnob9;
            case 7: return topKnob10;
            default: return -1;
        }
    }

    static bool isAboveScreenEncoderCcNumber(int ccNumber) {
        return ccNumber >= topKnob3 && ccNumber <= topKnob10;
    }

    static bool isButtonPressControlMessage(const MidiMessage &message) {
        return message.getControllerValue() == 127;
    }

    static bool isButtonReleaseControlMessage(const MidiMessage &message) {
        return message.getControllerValue() == 0;
    }
};
