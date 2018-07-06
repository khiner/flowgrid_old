#pragma once
#include "JuceHeader.h"

namespace {
    bool matchSubStringIgnoreCase(const std::string &haystack, const std::string &needle) {
        auto it = std::search(
                haystack.begin(), haystack.end(),
                needle.begin(), needle.end(),
                [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
        return it != haystack.end();
    }
}

class MidiCommunicator: public MidiInputCallback  {
public:
    using midi_callback_t = std::function<void(const MidiMessage &)>;

    explicit MidiCommunicator(const std::string &deviceName);

    inline bool isConnected() {
        return !midiInput->getDevices().isEmpty();
    }

    static int findDeviceIdByDeviceName(const std::string &deviceName) {
        auto devices = MidiInput::getDevices();
        int index = 0;
        for (auto &device: devices) {
            if (matchSubStringIgnoreCase(device.toStdString(), deviceName)) {
                return index;
            }
            index++;
        }

        return -1;
    }

    void setMidiInputCallback(const midi_callback_t &midiCallback) {
        this->midiCallback = midiCallback;
    }

    /*!
     *  The Juce MIDI incoming message callback
     *  @see juce::MidiInputCallback
     */
    void handleIncomingMidiMessage(MidiInput * /*source*/, const MidiMessage &message) override {
        if (midiCallback) {
            midiCallback(message);
        }
    }
protected:
    std::string deviceName;
    std::unique_ptr<MidiInput> midiInput;
    std::unique_ptr<MidiOutput> midiOutput;
    midi_callback_t midiCallback;
};
