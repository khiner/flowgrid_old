#pragma once
#include "JuceHeader.h"

class MidiCommunicator: public MidiInputCallback, private Timer {
public:
    using midi_callback_t = std::function<void(const MidiMessage &)>;

    explicit MidiCommunicator(const String &deviceName) {
        this->deviceName = deviceName;
        if (!findDeviceAndStart())
            startTimer(1000); // check back every second
    }

    inline bool isInputConnected() const {
        return midiInput && midiInput->getDevices().contains(deviceName);
    }

    inline bool isOutputConnected() const {
        return midiOutput != nullptr;
    }

    static int findDeviceIdByDeviceName(const String &deviceName) {
        auto devices = MidiInput::getDevices();
        for (int i = 0; i < devices.size(); i++) {
            if (devices[i].containsIgnoreCase(deviceName))
                return i;
        }

        return -1;
    }

    void setMidiInputCallback(const midi_callback_t &midiCallback) {
        this->midiCallback = midiCallback;
    }

    void handleIncomingMidiMessage(MidiInput * /*source*/, const MidiMessage &message) override {
        if (midiCallback) {
            midiCallback(message);
        }
    }
protected:
    String deviceName;
    std::unique_ptr<MidiInput> midiInput;
    std::unique_ptr<MidiOutput> midiOutput;
    midi_callback_t midiCallback;

    virtual bool findDeviceAndStart() {
        int deviceIndex = findDeviceIdByDeviceName(deviceName);
        if (deviceIndex == -1)
            return false;

        midiInput.reset(MidiInput::openDevice(deviceIndex, this));
        if (!midiInput)
            throw std::runtime_error("Failed to open MIDI input device");

        midiOutput.reset(MidiOutput::openDevice(deviceIndex));
        if (!midiOutput)
            throw std::runtime_error("Failed to open MIDI output device");

        midiInput->start();

        return true;
    }

private:
    void timerCallback() override {
        if (findDeviceAndStart())
            stopTimer();
    }
};
