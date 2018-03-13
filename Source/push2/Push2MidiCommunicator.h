#pragma once

#include <midi/MidiCommunicator.h>

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
};
