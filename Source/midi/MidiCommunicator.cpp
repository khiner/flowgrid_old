#include "MidiCommunicator.h"

using namespace std;

MidiCommunicator::MidiCommunicator(const string &deviceName): deviceName(deviceName) {
    int deviceIndex = findDeviceIdByDeviceName(deviceName);
    if (deviceIndex == -1) {
        throw runtime_error("Failed to find input midi device with device name " + deviceName);
    }
    midiInput.reset(MidiInput::openDevice(deviceIndex, this));

    if (!midiInput) {
        throw runtime_error("Failed to open MIDI input device");
    }
    midiOutput.reset(MidiOutput::openDevice(deviceIndex));

    if (!midiOutput) {
        throw runtime_error("Failed to open MIDI output device");
    }
    midiInput->start();
}
