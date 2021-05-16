#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

using namespace juce;

class MidiCommunicator : public MidiInputCallback {
public:
    explicit MidiCommunicator() = default;

    void setMidiInputAndOutput(std::unique_ptr<MidiInput> midiInput, std::unique_ptr<MidiOutput> midiOutput) {
        this->midiInput = std::move(midiInput);
        this->midiOutput = std::move(midiOutput);
        if (this->midiInput != nullptr && this->midiOutput != nullptr) {
            initialized = true;
            initialize();
        } else {
            initialized = false;
        }
    }

    bool isInitialized() const { return initialized; }

    void addMidiInputCallback(MidiInputCallback *callbackToAdd) {
        const ScopedLock sl(midiCallbackLock);
        midiCallbacks.add(callbackToAdd);
    }

    void removeMidiInputCallback(MidiInputCallback *callbackToRemove) {
        const ScopedLock sl(midiCallbackLock);
        midiCallbacks.removeAllInstancesOf(callbackToRemove);
    }

    bool isOutputConnected() const { return midiOutput != nullptr; }

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
