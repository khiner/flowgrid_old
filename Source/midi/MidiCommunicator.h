#pragma once
#include "JuceHeader.h"

class MidiCommunicator: public MidiInputCallback {
public:
    explicit MidiCommunicator() = default;

    void setMidiInputAndOutput(MidiInput* midiInput, MidiOutput* midiOutput) {
        this->midiInput.reset(midiInput);
        this->midiOutput.reset(midiOutput);
        if (midiInput != nullptr && midiOutput != nullptr) {
            initialized = true;
            initialize();
        } else {
            initialized = false;
        }
    }

    bool isInitialized() { return initialized; }

    inline bool isInputConnected() const {
        return midiInput && midiInput->getDevices().contains(deviceName);
    }

    inline bool isOutputConnected() const {
        return midiOutput != nullptr;
    }
protected:
    String deviceName;
    std::unique_ptr<MidiInput> midiInput;
    std::unique_ptr<MidiOutput> midiOutput;

    virtual void initialize() { midiInput->start(); }

private:
    bool initialized { false };
};
