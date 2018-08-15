#pragma once

#include "JuceHeader.h"

class MidiCommunicator : public MidiInputCallback {
public:
    explicit MidiCommunicator() = default;

    void setMidiInputAndOutput(MidiInput *midiInput, MidiOutput *midiOutput) {
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

    void addMidiInputCallback(MidiInputCallback *callbackToAdd) {
        const ScopedLock sl(midiCallbackLock);
        midiCallbacks.add(callbackToAdd);
    }

    void removeMidiInputCallback(MidiInputCallback *callbackToRemove) {
        const ScopedLock sl(midiCallbackLock);
        midiCallbacks.removeAllInstancesOf(callbackToRemove);
    }

    inline bool isInputConnected() const {
        return midiInput && midiInput->getDevices().contains(deviceName);
    }

    inline bool isOutputConnected() const {
        return midiOutput != nullptr;
    }

    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override {
        if (!message.isActiveSense()) {
            const ScopedLock sl(midiCallbackLock);

            for (auto &mc : midiCallbacks)
                mc->handleIncomingMidiMessage(source, message);
        }
    }

protected:
    String deviceName;
    std::unique_ptr<MidiInput> midiInput;
    std::unique_ptr<MidiOutput> midiOutput;

    Array<MidiInputCallback *> midiCallbacks;
    CriticalSection midiCallbackLock;

    virtual void initialize() { midiInput->start(); }

private:
    bool initialized{false};
};
