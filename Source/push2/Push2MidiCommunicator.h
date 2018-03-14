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

    enum ControlLabel {
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

    const static std::unordered_map<ControlLabel, int> ccNumberForControlLabel;
};
