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

    const enum ControlLabel {
        topKnob1, topKnob2, topKnob3, topKnob4, topKnob5, topKnob6, topKnob7, topKnob8, topKnob9, topKnob10, masterKnob,

        tapTempo, metronome, setup, user,

        topDisplayButton1, topDisplayButton2, topDisplayButton3, topDisplayButton4, topDisplayButton5, topDisplayButton6, topDisplayButton7, topDisplayButton8,

        bottomDisplayButton1, bottomDisplayButton2, bottomDisplayButton3, bottomDisplayButton4, bottomDisplayButton5, bottomDisplayButton6, bottomDisplayButton7, bottomDisplayButton8,

        delele_, undo, addDevice, addTrack, device, mix, browse, clip,

        mute, solo, stopClip, master,

        convert, doubleLoop, quantize, duplicate, new_, fixedLength, automate, record, play,

        divide_1_32t, divide_1_32, divide_1_16_t, divide_1_16, divide_1_8_t, divide_1_8, divide_1_4t, divide_1_4,

        left, right, up, down,

        repeat, accent, scale, layout, note, session,

        octaveUp, octaveDown, pageLeft, pageRight,

        shift, select,
    };

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
        if (message.getRawDataSize() != 3)
            throw std::runtime_error("Expected CC message to have 24 bits");

        const uint8 *rawData = message.getRawData();
        const uint8 byteValue = *(rawData + 2);
        if (byteValue <= 63) {
            return static_cast<float>(byteValue) / 210.0f;
        } else {
            return (static_cast<float>(byteValue) - 128.0f) / 210.0f;
        }
    }

    static int ccNumberForTopKnobIndex(int topKnobIndex) {
        switch (topKnobIndex) {
            case 0: return ccNumberForControlLabel.at(topKnob3);
            case 1: return ccNumberForControlLabel.at(topKnob4);
            case 2: return ccNumberForControlLabel.at(topKnob5);
            case 3: return ccNumberForControlLabel.at(topKnob6);
            case 4: return ccNumberForControlLabel.at(topKnob7);
            case 5: return ccNumberForControlLabel.at(topKnob8);
            case 6: return ccNumberForControlLabel.at(topKnob9);
            case 7: return ccNumberForControlLabel.at(topKnob10);
            default: return -1;
        }
    }

    const static std::unordered_map<ControlLabel, int> ccNumberForControlLabel;
};
